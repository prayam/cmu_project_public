#include "faceNet.h"
#include "CommonStruct.h"
#include <glib.h>
#include "Logger.h"

gint FaceNetClassifier::m_classCount = 0;

FaceNetClassifier::FaceNetClassifier
(Logger gLogger, DataType dtype, const string& uffFile, const string& engineFile, gint batchSize, gboolean serializeEngine,
 float knownPersonThreshold, gint maxFacesPerScene, gint frameWidth, gint frameHeight):
	m_INPUT_C(static_cast<const gint>(3)),
	m_INPUT_H(static_cast<const gint>(160)),
	m_INPUT_W(static_cast<const gint>(160)),
	m_frameWidth(static_cast<const gint>(frameWidth)),
	m_frameHeight(static_cast<const gint>(frameHeight)),
	m_gLogger(gLogger),
	m_dtype(dtype),
	m_uffFile(static_cast<const string>(uffFile)),
	m_engineFile(static_cast<const string>(engineFile)),
	m_batchSize(batchSize),
	m_serializeEngine(serializeEngine),
	m_maxFacesPerScene(maxFacesPerScene),
	m_knownPersonThresh(knownPersonThreshold)
{
	m_croppedFaces.reserve(maxFacesPerScene);
	m_embeddings.reserve(128);
	m_output[0] = 0.0f;

	// load engine from .engine file or create new engine
	this->createOrLoadEngine();
}

#define DONOT_USE_DEPRECATED_FUNCTION
void FaceNetClassifier::createOrLoadEngine() {
	if(fileExists(m_engineFile)) {
		std::vector<gchar> trtModelStream_;
		gsize size{ 0 };

		std::ifstream file(m_engineFile, std::ios::binary);
		if (file.good())
		{
			file.seekg(0, file.end);
			size = file.tellg();
			file.seekg(0, file.beg);
			trtModelStream_.resize(size);
			std::cout << "size" << trtModelStream_.size() << std::endl;
			file.read(trtModelStream_.data(), size);
			file.close();
		}
		// std::cout << "size" << size;
		IRuntime* runtime = createInferRuntime(m_gLogger);
		assert(runtime != nullptr);
		m_engine = runtime->deserializeCudaEngine(trtModelStream_.data(), size, nullptr);
		std::cout << std::endl;
	}
	else {
		IBuilder *builder = createInferBuilder(m_gLogger);
#ifndef DONOT_USE_DEPRECATED_FUNCTION
		INetworkDefinition *network = builder->createNetwork();
#else
		INetworkDefinition *network = builder->createNetworkV2(0U);
		IBuilderConfig *config = builder->createBuilderConfig();
#endif
		IUffParser *parser = createUffParser();
#ifndef DONOT_USE_DEPRECATED_FUNCTION
		parser->registerInput("input", DimsCHW(160, 160, 3), UffInputOrder::kNHWC);
#else
		parser->registerInput("input", Dims3(160, 160, 3), UffInputOrder::kNHWC);
#endif
		parser->registerOutput("embeddings");

		if (!parser->parse(m_uffFile.c_str(), *network, m_dtype))
		{
			cout << "Failed to parse UFF\n";
			builder->destroy();
			parser->destroy();
			network->destroy();
			throw std::exception();
		}

		/* build engine */
		if (m_dtype == DataType::kHALF)
		{
#ifndef DONOT_USE_DEPRECATED_FUNCTION
			builder->setFp16Mode(true);
#else
			config->setFlag(BuilderFlag::kFP16);
#endif
		}
		else if (m_dtype == DataType::kINT8) {
#ifndef DONOT_USE_DEPRECATED_FUNCTION
			builder->setInt8Mode(true);
			// ToDo
			//builder->setInt8Calibrator()
#else
			config->setFlag(BuilderFlag::kINT8);
#endif
		}
		builder->setMaxBatchSize(m_batchSize);
#ifndef DONOT_USE_DEPRECATED_FUNCTION
		builder->setMaxWorkspaceSize(1<<30);
#else
		config->setMaxWorkspaceSize(1<<30);
#endif
		// strict will force selected datatype, even when another was faster
		//builder->setStrictTypeConstraints(true);
		// Disable DLA, because many layers are still not supported
		// and this causes additional latency.
		//builder->allowGPUFallback(true);
		//builder->setDefaultDeviceType(DeviceType::kDLA);
		//builder->setDLACore(1);
#ifndef DONOT_USE_DEPRECATED_FUNCTION
		m_engine = builder->buildCudaEngine(*network);
#else
		m_engine = builder->buildEngineWithConfig(*network, *config);
#endif
		/* serialize engine and write to file */
		if(m_serializeEngine) {
			ofstream planFile;
			planFile.exceptions(ofstream::failbit | ofstream::badbit);
			try {
				planFile.open(m_engineFile);
				if (planFile.is_open()) {
					IHostMemory *serializedEngine = m_engine->serialize();
					planFile.write((gchar *) serializedEngine->data(), serializedEngine->size());
					planFile.close();

				}
			}
			catch(ofstream::failure &e) {
				std::cerr << "Exception opening/writing/closing file\n";
			}
		}

		/* break down */
		builder->destroy();
		parser->destroy();
		network->destroy();
	}
	m_context = m_engine->createExecutionContext();
}


void FaceNetClassifier::getCroppedFacesAndAlign(cv::Mat frame, std::vector<struct Bbox> outputBbox) {
	for(vector<struct Bbox>::iterator it=outputBbox.begin(); it!=outputBbox.end();++it){
		if((*it).exist){
			cv::Rect facePos(cv::Point((*it).y1, (*it).x1), cv::Point((*it).y2, (*it).x2));
			cv::Mat tempCrop = frame(facePos);
			struct CroppedFace currFace;
			cv::resize(tempCrop, currFace.faceMat, cv::Size(160, 160), 0, 0, cv::INTER_CUBIC);
			currFace.x1 = it->x1;
			currFace.y1 = it->y1;
			currFace.x2 = it->x2;
			currFace.y2 = it->y2;
			m_croppedFaces.push_back(currFace);
		}
	}
	//ToDo align
}

void FaceNetClassifier::preprocessFaces() {
	// preprocess according to facenet training and flatten for input to runtime engine
	for (gsize i = 0; i < m_croppedFaces.size(); i++) {
		//mean and std
		cv::cvtColor(m_croppedFaces[i].faceMat, m_croppedFaces[i].faceMat, cv::COLOR_RGB2BGR);
		cv::Mat temp = m_croppedFaces[i].faceMat.reshape(1, m_croppedFaces[i].faceMat.rows * 3);
		cv::Mat mean3;
		cv::Mat stddev3;
		cv::meanStdDev(temp, mean3, stddev3);

		double mean_pxl = mean3.at<double>(0);
		double stddev_pxl = stddev3.at<double>(0);
		cv::Mat image2;
		m_croppedFaces[i].faceMat.convertTo(image2, CV_64FC1);
		m_croppedFaces[i].faceMat = image2;
		// fix by peererror
		cv::Mat mat(4, 1, CV_64FC1);
		mat.at <double>(0, 0) = mean_pxl;
		mat.at <double>(1, 0) = mean_pxl;
		mat.at <double>(2, 0) = mean_pxl;
		mat.at <double>(3, 0) = 0;
		m_croppedFaces[i].faceMat = m_croppedFaces[i].faceMat - mat;
		// end fix
		m_croppedFaces[i].faceMat = m_croppedFaces[i].faceMat / stddev_pxl;
		m_croppedFaces[i].faceMat.convertTo(image2, CV_32FC3);
		m_croppedFaces[i].faceMat = image2;
	}
}


void FaceNetClassifier::doInference(float* inputData, float* output) {
	gint size_of_single_input = 3 * 160 * 160 * sizeof(float);
	gint size_of_single_output = 128 * sizeof(float);
	gint inputIndex = m_engine->getBindingIndex("input");
	gint outputIndex = m_engine->getBindingIndex("embeddings");

	void* buffers[2];

	cudaMalloc(&buffers[inputIndex], m_batchSize * size_of_single_input);
	cudaMalloc(&buffers[outputIndex], m_batchSize * size_of_single_output);

	cudaStream_t stream;
	CHECK(cudaStreamCreate(&stream));

	// copy data to GPU and execute
	CHECK(cudaMemcpyAsync(buffers[inputIndex], inputData, m_batchSize * size_of_single_input, cudaMemcpyHostToDevice, stream));
	m_context->enqueue(m_batchSize, &buffers[0], stream, nullptr);
	CHECK(cudaMemcpyAsync(output, buffers[outputIndex], m_batchSize * size_of_single_output, cudaMemcpyDeviceToHost, stream));
	cudaStreamSynchronize(stream);

	// Release the stream and the buffers
	cudaStreamDestroy(stream);
	CHECK(cudaFree(buffers[inputIndex]));
	CHECK(cudaFree(buffers[outputIndex]));
}


void FaceNetClassifier::forwardAddFace(cv::Mat image, const std::vector<struct Bbox>& outputBbox,
		const string& className) {

	//cv::resize(image, image, cv::Size(1280, 720), 0, 0, cv::INTER_CUBIC);
	getCroppedFacesAndAlign(image, outputBbox);
	if(!m_croppedFaces.empty()) {
		preprocessFaces();
		doInference((float*)m_croppedFaces[0].faceMat.ptr<float>(0), m_output);
		struct KnownID person;
		person.className = className;
		person.classNumber = m_classCount;
		person.embeddedFace.insert(person.embeddedFace.begin(), m_output, m_output+128);
		m_knownFaces.push_back(person);
		m_classCount++;
	}
	m_croppedFaces.clear();
}

void FaceNetClassifier::forward(cv::Mat frame, const std::vector<struct Bbox>& outputBbox) {
	getCroppedFacesAndAlign(frame, outputBbox); // ToDo align faces according to points
	preprocessFaces();
	for(gsize i = 0; i < m_croppedFaces.size(); i++) {
		doInference((float*)m_croppedFaces[i].faceMat.ptr<float>(0), m_output);
		m_embeddings.insert(m_embeddings.end(), m_output, m_output+128);
	}
}

void FaceNetClassifier::featureMatching(const cv::Mat &image, std::vector<struct APP_meta> &meta) {
	(void) image;

	for(gsize i = 0; i < (m_embeddings.size()/128); i++) {
		double minDistance = 10.* m_knownPersonThresh;
		gsize winner = 0;
		gboolean hasWinner = false;
		for (gsize j = 0; j < m_knownFaces.size(); j++) {
			float currDistance = 0.;
			std::vector<float> currEmbedding(128);
			std::copy_n(m_embeddings.begin()+(i*128), 128, currEmbedding.begin());
			currDistance = vectors_distance(currEmbedding, m_knownFaces[j].embeddedFace);
			// printf("The distance to %s is %.10f \n", m_knownFaces[j].className.c_str(), currDistance);
			// if ((currDistance < m_knownPersonThresh) && (currDistance < minDistance)) {
			if (currDistance < minDistance) {
				minDistance = currDistance;
				winner = j;
				hasWinner = true;
			}
			currEmbedding.clear();
		}
		// float fontScaler = static_cast<float>(m_croppedFaces[i].x2 - m_croppedFaces[i].x1)/static_cast<float>(m_frameWidth);
		// cv::rectangle(image, cv::Point(m_croppedFaces[i].y1, m_croppedFaces[i].x1), cv::Point(m_croppedFaces[i].y2, m_croppedFaces[i].x2),
		//		cv::Scalar(0,0,255), 2,8,0);
		if (minDistance <= m_knownPersonThresh) {
			// cv::putText(image, m_knownFaces[winner].className, cv::Point(m_croppedFaces[i].y1+2, m_croppedFaces[i].x2-3),
			//         cv::FONT_HERSHEY_DUPLEX, 0.1 + 2*fontScaler*4,  cv::Scalar(0,0,255,255), 1);
			//cv::putText(image, m_knownFaces[winner].className, cv::Point(m_croppedFaces[i].y1+2, m_croppedFaces[i].x2-3),
			//		cv::FONT_HERSHEY_DUPLEX, 0.1 + 2*fontScaler*4,  cv::Scalar(0,0,255,255), 1);
			struct APP_meta tmp;
			g_strlcpy(tmp.name, m_knownFaces[winner].className.c_str(), MAX_NAME - 1);
			tmp.x1 = m_croppedFaces[i].x1;
			tmp.y1 = m_croppedFaces[i].y1;
			tmp.x2 = m_croppedFaces[i].x2;
			tmp.y2 = m_croppedFaces[i].y2;
			meta.push_back(tmp);
		}
		else if (minDistance > m_knownPersonThresh || !hasWinner){
			//cv::putText(image, "New Person", cv::Point(m_croppedFaces[i].y1+2, m_croppedFaces[i].x2-3),
			//        cv::FONT_HERSHEY_DUPLEX, 0.1 + 2*fontScaler*4 ,  cv::Scalar(0,0,255,255), 1);
			//cv::putText(image, "New Person", cv::Point(m_croppedFaces[i].y1+2, m_croppedFaces[i].x2-3),
			//		cv::FONT_HERSHEY_DUPLEX, 0.1 + 2*fontScaler*4 ,  cv::Scalar(0,0,255,255), 1);
			struct APP_meta tmp;
			g_strlcpy(tmp.name, "New Person", MAX_NAME - 1);
			tmp.x1 = m_croppedFaces[i].x1;
			tmp.y1 = m_croppedFaces[i].y1;
			tmp.x2 = m_croppedFaces[i].x2;
			tmp.y2 = m_croppedFaces[i].y2;
			meta.push_back(tmp);
		}
	}
}

#include "certcheck.h"
gint load_dec_cvimage(cv::Mat &image, std::string &name, const char *fn)
{
	unsigned char *buf;
	size_t size;
	gint ret = dec_ssl_fm(fn, &buf, &size);

	if (ret) {
		LOG_WARNING("Decrypting and loading jpg image failed\n");
		return ret;
	}

	name = std::string((const char *)buf);
	cv::_InputArray pic_arr(buf + strlen((const char *)buf) + 1, size - 1 - strlen((const char *)buf));
	image = cv::imdecode(pic_arr, cv::IMREAD_COLOR);

	free(buf);
	return 0;
}

void save_enc_cvimage(cv::Mat &image, std::string name)
{
	std::vector<uchar> buf(name.begin(), name.end());
	std::vector<uchar> pic_buf;
	cv::imencode(".jpg", image, pic_buf);
	buf.push_back('\0');
	buf.insert(buf.end(), pic_buf.begin(), pic_buf.end());

	gchar *path = NULL;
	gchar *fn = (gchar*)g_malloc(34);
	if (fn == NULL) {
		LOG_WARNING("g_malloc failed");
		return;
	}
	getFileName_leng32(fn);
	path = g_strdup_printf ("./asset/imgs/%s", fn);
	gint ret = enc_ssl_mf(buf.data(), buf.size(), path);
	if (ret)
		LOG_WARNING("Encrypting and saving jpg image failed\n");

	g_free(fn);
	g_free(path);
}

void FaceNetClassifier::addNewFace(cv::Mat &image, const std::vector<struct Bbox>& outputBbox) {
	std::cout << "Adding new person...\nPlease make sure there is only one face in the current frame.\n"
		<< "What's your name? ";
	string newName;
	std::cin >> newName;
	std::cout << "Hi " << newName << ", you will be added to the database.\n";
	forwardAddFace(image, outputBbox, newName);
	save_enc_cvimage(image, newName);
}

void FaceNetClassifier::addNewFace_name(cv::Mat &image, const std::vector<struct Bbox>& outputBbox, const string& newName) {
	forwardAddFace(image, outputBbox, newName);
	save_enc_cvimage(image, newName);
}

void FaceNetClassifier::resetVariables() {
	m_embeddings.clear();
	m_croppedFaces.clear();
}

FaceNetClassifier::~FaceNetClassifier() {
	// this leads to segfault
	// this->m_engine->destroy();
	// this->m_context->destroy();
	// std::cout << "FaceNet was destructed" << std::endl;
}


// HELPER FUNCTIONS
// Computes the distance between two std::vectors
float vectors_distance(const std::vector<float>& a, const std::vector<float>& b) {
	std::vector<double>	auxiliary;
	std::transform (a.begin(), a.end(), b.begin(), std::back_inserter(auxiliary),//
			[](float element1, float element2) {return pow((element1-element2),2);});
	auxiliary.shrink_to_fit();
	float loopSum = 0.;
	for(auto it=auxiliary.begin(); it!=auxiliary.end(); ++it) loopSum += *it;

	return  std::sqrt(loopSum);
}



inline guint elementSize(nvinfer1::DataType t)
{
	switch (t)
	{
		case nvinfer1::DataType::kINT32:
			// Fallthrough, same as kFLOAT
		case nvinfer1::DataType::kFLOAT: return 4;
		case nvinfer1::DataType::kHALF: return 2;
		case nvinfer1::DataType::kINT8: return 1;
		default: break;
	}
	assert(0);
	return 0;
}
