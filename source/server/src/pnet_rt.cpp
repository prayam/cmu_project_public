//
// Created by zhou on 18-4-30.
//
#include "pnet_rt.h"
#include <fstream>

// stuff we know about the network and the caffe input/output blobs
Pnet_engine::Pnet_engine() : baseEngine("./asset/mtCNNModels/det1_relu.prototxt",
		"./asset/mtCNNModels/det1_relu.caffemodel",
		"data",
		"conv4-2",
		"prob1") {
};

Pnet_engine::~Pnet_engine() {
	shutdownProtobufLibrary();
}

void Pnet_engine::init(gint row, gint col) {
	//modifiy the input shape of prototxt, write to temp.prototxt
	gint first_spce = 16, second_space = 4;
	fstream protofile;
	protofile.exceptions(fstream::failbit | fstream::badbit);
	try {
		protofile.open(prototxt, ios::in);
		std::string contents("");
		if (protofile.is_open()) {
			std::stringstream buffer;
			buffer << protofile.rdbuf();
			contents = std::string(buffer.str());
			//    std::cout << "contents = " << contents << std::endl;
			string::size_type position_h, position_w;

			(void) row;
			(void) col;

			position_h = contents.find("dim");
			while (isdigit(contents[position_h + first_spce])) {
				contents.erase(position_h + first_spce, 1);
			}
			contents.insert(position_h + first_spce, to_string(row));
			position_w = contents.find("dim", position_h + first_spce);
			while (isdigit(contents[position_w + second_space])) {
				contents.erase(position_w + second_space, 1);
			}
			contents.insert(position_w + second_space, to_string(col));
			protofile.close();
		}
		protofile.open("temp.prototxt", ios::out);
		if (protofile.is_open()) {
			protofile.write(contents.c_str(), contents.size());
			protofile.close();
		}
	}
	catch(fstream::failure &e) {
		std::cerr << "Exception opening/writing/closing file\n";
	}
	IHostMemory *gieModelStream{nullptr};
	//generate Tensorrt model
	caffeToGIEModel("temp.prototxt", model, std::vector<std::string>{OUTPUT_PROB_NAME, OUTPUT_LOCATION_NAME}, 1,
			gieModelStream);

}


Pnet::Pnet(gint row, gint col, const Pnet_engine &pnet_engine) : BatchSize(1),
	INPUT_C(3), Engine(pnet_engine.context->getEngine()), INPUT_H(row), INPUT_W(col) {
		Pthreshold = 0.6;
		nms_threshold = 0.5;
		this->score_ = new pBox;
		this->location_ = new pBox;
		this->rgb = new pBox;
		//calculate output shape
		this->score_->width = gint(ceil((INPUT_W - 2) / 2.) - 4);
		this->score_->height = gint(ceil((INPUT_H - 2) / 2.) - 4);
		this->score_->channel = 2;

		this->location_->width = gint(ceil((INPUT_W - 2) / 2.) - 4);
		this->location_->height = gint(ceil((INPUT_H - 2) / 2.) - 4);
		this->location_->channel = 4;

		OUT_PROB_SIZE = this->score_->width * this->score_->height * this->score_->channel;
		OUT_LOCATION_SIZE = this->location_->width * this->location_->height * this->location_->channel;
		//allocate memory for outputs
		this->rgb->pdata = (float *) malloc(INPUT_C * INPUT_H * INPUT_W * sizeof(float));
		this->score_->pdata = (float *) malloc(OUT_PROB_SIZE * sizeof(float));
		this->location_->pdata = (float *) malloc(OUT_LOCATION_SIZE * sizeof(float));

		assert(Engine.getNbBindings() == 3);
		inputIndex = Engine.getBindingIndex(pnet_engine.INPUT_BLOB_NAME),
			   outputProb = Engine.getBindingIndex(pnet_engine.OUTPUT_PROB_NAME),
			   outputLocation = Engine.getBindingIndex(pnet_engine.OUTPUT_LOCATION_NAME);

		//creat GPU buffers and stream
		CHECK(cudaMalloc(&buffers[inputIndex], BatchSize * INPUT_C * INPUT_H * INPUT_W * sizeof(float)));
		CHECK(cudaMalloc(&buffers[outputProb], BatchSize * OUT_PROB_SIZE * sizeof(float)));
		CHECK(cudaMalloc(&buffers[outputLocation], BatchSize * OUT_LOCATION_SIZE * sizeof(float)));
		CHECK(cudaStreamCreate(&stream));
	}

Pnet::~Pnet() {

	delete (score_);
	delete (location_);

	cudaStreamDestroy(stream);
	CHECK(cudaFree(buffers[inputIndex]));
	CHECK(cudaFree(buffers[outputProb]));
	CHECK(cudaFree(buffers[outputLocation]));
}

void Pnet::run(const cv::Mat &image, float scale, const Pnet_engine &pnet_engine) {


	//DMA the input to the GPU ,execute the batch asynchronously and DMA it back;
	image2Matrix(image, this->rgb);
	CHECK(cudaMemcpyAsync(buffers[inputIndex], this->rgb->pdata,
				BatchSize * INPUT_C * INPUT_H * INPUT_W * sizeof(float),
				cudaMemcpyHostToDevice, stream));
	pnet_engine.context->enqueue(BatchSize, buffers, stream, nullptr);
	CHECK(cudaMemcpyAsync(this->score_->pdata, buffers[outputProb], BatchSize * OUT_PROB_SIZE * sizeof(float),
				cudaMemcpyDeviceToHost, stream));
	CHECK(cudaMemcpyAsync(this->location_->pdata, buffers[outputLocation],
				BatchSize * OUT_LOCATION_SIZE * sizeof(float), cudaMemcpyDeviceToHost, stream));
	cudaStreamSynchronize(stream);
	generateBbox(this->score_, this->location_, scale);

}

void Pnet::generateBbox(const struct pBox *score, const struct pBox *location, mydataFmt scale) {
	//for pooling
	gint stride = 2;
	gint cellsize = 12;
	gint count = 0;
	//score p
	mydataFmt *p = score->pdata + score->width * score->height;
	mydataFmt *plocal = location->pdata;
	struct Bbox bbox;
	struct orderScore order;
	for (gint row = 0; row < score->height; row++) {
		for (gint col = 0; col < score->width; col++) {
			if (*p > Pthreshold) {
				bbox.score = *p;
				order.score = *p;
				order.oriOrder = count;
				bbox.x1 = round((stride * row + 1) / scale);
				bbox.y1 = round((stride * col + 1) / scale);
				bbox.x2 = round((stride * row + 1 + cellsize) / scale);
				bbox.y2 = round((stride * col + 1 + cellsize) / scale);
				bbox.exist = true;
				bbox.area = (bbox.x2 - bbox.x1) * (bbox.y2 - bbox.y1);
				for (gint channel = 0; channel < 4; channel++)
					bbox.regreCoord[channel] = *(plocal + channel * location->width * location->height);
				boundingBox_.push_back(bbox);
				bboxScore_.push_back(order);
				count++;
			}
			p++;
			plocal++;
		}
	}

}
