#include "app.h"
#include "TcpSendRecv.h"
#include "CommonStruct.h"
#include "Logger.h"
#include "certcheck.h"
#include <iostream>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

// Label: https://developer.gnome.org/gtkmm/3.24/classGtk_1_1Label.html
// Box: https://developer.gnome.org/gtkmm/3.24/classGtk_1_1Box.html
// CheckButton: https://developer.gnome.org/gtkmm/3.24/classGtk_1_1CheckButton.html
// Button: https://developer.gnome.org/gtkmm/3.24/classGtk_1_1Button.html
// Entry: https://developer.gnome.org/gtkmm/3.24/classGtk_1_1Entry.html

using namespace cv;
using namespace std;

static gint readysocket(gint fd)
{
	struct timeval tv = { 0L, 0L };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	return select(fd + 1, &fds, NULL, NULL, &tv);
}

static gboolean handle_port_secure(gpointer data) {
	App *app = static_cast<App *>(data);
	Mat image;
	float fontScaler;
	std::vector<struct APP_meta> meta_vector;
	gssize ret;
	static CLIENT_STATE prev_state = CLIENT_STATE_SECURE_RUN;
	gint skip_picture_count = 0;

	while (app->connected_server) {
		gboolean recv_meta = FALSE, recv_photo = FALSE;
		CLIENT_STATE current_state = app->get_current_app_state();
		if (prev_state != current_state) {
			// if mode is changed between *secure* and *non-secure*,
			//  1. client request the change mode to sever and change the receiving channel
			//  2. server change the sending channel after receiving the rqeuest
			// so, the some data is remain in the tcp recv buffer since the time interval between step 1~2.
			// to prevent showing the date received 1~2, skip some pictures.
			LOG_WARNING("enable skipping pictures");

			if (current_state == CLIENT_STATE_SECURE_RUN || current_state == CLIENT_STATE_NONSECURE_RUN) {
				skip_picture_count = 15;
			}
			else if (current_state == CLIENT_STATE_SECURE_TESTRUN || current_state == CLIENT_STATE_NONSECURE_TESTRUN) {
				skip_picture_count = 6;
			}
		}
		prev_state = current_state;

recv:
		recv_meta = FALSE;
		recv_photo = FALSE;

		if (app->port_recv_photo != NULL) {
			gint fd = 0;
			if (app->port_recv_photo->isSsl) {
				fd = SSL_get_fd(app->port_recv_photo->ssl);
			}
			else {
				fd = app->port_recv_photo->ConnectedFd;
			}

			if (app->get_current_app_state() == CLIENT_STATE_LEARN && app->learn_mode_state == LEARN_REQUESTED) {
				meta_vector.clear();

				// recv photo
				do {
					ret = TcpRecvImageAsJpeg(app->port_recv_photo, &image);
					if (ret == TCP_RECV_PEER_DISCONNECTED || ret == TCP_RECV_ERROR) {
						LOG_WARNING("TcpRecvImageAsJpeg fail");
						break;
					}
					else if (ret == TCP_RECV_TIMEOUT) {
						break;
					}
					else {
						recv_photo = TRUE;
					}

					if (readysocket(SSL_get_fd(app->port_meta->ssl))) {
						ret = TcpRecvMeta(app->port_meta, meta_vector);
						if (ret == TCP_RECV_PEER_DISCONNECTED || ret == TCP_RECV_ERROR) {
							LOG_WARNING("TcpRecvMeta fail");
						}
						else if (ret == TCP_RECV_TIMEOUT) {
							// waitKey(10);
							// goto recv;
						}
						else {
							recv_meta = TRUE;
						}
					}
				} while (readysocket(fd));

				// recv meta
				while (readysocket(SSL_get_fd(app->port_meta->ssl))) {
					meta_vector.clear();
					ret = TcpRecvMeta(app->port_meta, meta_vector);

					if (ret == TCP_RECV_PEER_DISCONNECTED || ret == TCP_RECV_ERROR) {
						LOG_WARNING("TcpRecvMeta fail");
						break;
					}
					else if (ret == TCP_RECV_TIMEOUT) {
						break;
					}
					else {
						recv_meta = TRUE;
					}
				}

				if(meta_vector.size() != 0) {
					meta_vector.resize(1);
					app->learn_mode_state = LEARN_READY;
					app->m_Entry_Name.set_sensitive(true);
					app->m_Label_Name.set_sensitive(true);
					LOG_INFO("Ready Learn");
				}
				else {
					app->learn_mode_state = LEARN_NOTREADY;
					app->m_Button_PauseResume.set_label("Resume\nneed to pause again\nto Save Picture");
					LOG_INFO("NotReady to Learn. need to pause again");
				}
			}
			else {
				// recv photo
				if (readysocket(fd)) {
					ret = TcpRecvImageAsJpeg(app->port_recv_photo, &image);
					if (ret == TCP_RECV_PEER_DISCONNECTED || ret == TCP_RECV_ERROR) {
						LOG_WARNING("TcpRecvImageAsJpeg fail");
						break;
					}
					else if (ret == TCP_RECV_TIMEOUT) {
						// waitKey(10);
						// goto recv;
					}
					else {
						recv_photo = TRUE;
					}
				}

				// recv meta
				if (readysocket(SSL_get_fd(app->port_meta->ssl))) {
					meta_vector.clear();
					ret = TcpRecvMeta(app->port_meta, meta_vector);
					if (ret == TCP_RECV_PEER_DISCONNECTED || ret == TCP_RECV_ERROR) {
						LOG_WARNING("TcpRecvMeta fail");
						break;
					}
					else if (ret == TCP_RECV_TIMEOUT) {
						// waitKey(10);
						// goto recv;
					}
					else {
						recv_meta = TRUE;
					}
				}

				if (skip_picture_count > 0) {
					if (recv_photo) {
						skip_picture_count--;
					}

					waitKey(10);
					goto recv;
				}
			}

			if (recv_photo) {
				cvtColor(image, image, COLOR_BGR2RGB);
			}

			for (struct APP_meta const & meta : meta_vector) {
				fontScaler = static_cast<float>(meta.x2 - meta.x1) / static_cast<float>(image.cols);
				putText(image, meta.name, Point(meta.y1 + 2, meta.x1 - 3),
					FONT_HERSHEY_DUPLEX, 0.1 + (2 * fontScaler * 3), Scalar(255, 0, 0, 255), 1);
				rectangle(image, Point(meta.y1, meta.x1), Point(meta.y2, meta.x2), Scalar(255, 0, 0), 2, 8, 0);
			}

			if (recv_photo || recv_meta) {
				app->m_Image.set(Gdk::Pixbuf::create_from_data(image.data, Gdk::COLORSPACE_RGB, false, 8, image.cols, image.rows, image.step));
			}
		}

		waitKey(10);
	}

	app->m_Image.clear();
	app->handle_recv_data = 0;
	app->on_button_logout();
	app->show_dialog("End Connection");
	return G_SOURCE_REMOVE;
}

App::App()
	: m_VBox_Left(Gtk::ORIENTATION_VERTICAL),
	  m_VBox_Right(Gtk::ORIENTATION_VERTICAL),
	  m_Button_Login("Login"),
	  m_Button_Logout("Logout"),
	  m_Button_PauseResume("Pause"),
	  m_Button_LearnSave("Learn Mode - Save"),
	  m_Label_Login("ID:      "),
	  m_Label_Password("Pass:  "),
	  m_Label_Name("Name:  "),
	  m_CheckButton_Secure("Secure Mode"),
	  m_CheckButton_Test("Test Mode")
{
	gsize len = 0;

	this->port_recv_photo = NULL;
	this->port_control = NULL;
	this->port_secure = NULL;
	this->port_nonsecure = NULL;
	this->port_meta = NULL;
	this->handle_recv_data = 0;
	this->connected_server = false;
	this->learn_mode_state = LEARN_NONE;
	this->remote_ip = NULL;

	if (0 != check_client_cert()) {
		LOG_WARNING("certificate error");
		exit(-1);
	}

	if (!g_file_get_contents("./remote.config", &this->remote_ip, &len, NULL)) {
		LOG_WARNING("cannot read remote.config file");
		exit(-1);
	}
	else {
		if (len < 7) {
			LOG_WARNING("not enough contents length in remote.config file");
			g_free(this->remote_ip);
			exit(-1);
		}

		g_strstrip(this->remote_ip);

		if (!g_hostname_is_ip_address(this->remote_ip)) {
			gchar *escape_str;

			escape_str = g_strescape(this->remote_ip, NULL);
			LOG_WARNING("not valid ipaddr in remote.config file %s", escape_str);

			g_free(this->remote_ip);
			g_free(escape_str);
			exit(-1);
		}

		LOG_INFO("remote ipaddr: %s", this->remote_ip);
	}

	set_title("Team6 Client App");

	add(m_HBox_Top);
	m_HBox_Top.add(m_VBox_Left);
	m_HBox_Top.add(m_VBox_Right);

	m_VBox_Left.add(m_HBox_LoginId);
	m_VBox_Left.add(m_HBox_LoginPassword);
	m_VBox_Left.add(m_HBox_LoginAction);

	m_Entry_Id.set_max_length(10);
	m_HBox_LoginId.add(m_Label_Login);
	m_HBox_LoginId.pack_start(m_Entry_Id);

	m_Entry_Password.set_max_length(20);
	m_HBox_LoginPassword.add(m_Label_Password);
	m_HBox_LoginPassword.pack_start(m_Entry_Password);

	m_HBox_LoginAction.pack_start(m_Button_Login);
	m_HBox_LoginAction.pack_start(m_Button_Logout);

	m_VBox_Left.pack_start(m_CheckButton_Secure);
	m_VBox_Left.pack_start(m_CheckButton_Test);
	m_VBox_Left.pack_start(m_Button_PauseResume);
	m_VBox_Left.add(m_HBox_Name);

	m_Entry_Name.set_max_length(20);
	m_HBox_Name.pack_start(m_Label_Name);
	m_HBox_Name.pack_start(m_Entry_Name);

	m_VBox_Left.pack_start(m_Button_LearnSave);

	m_CheckButton_Secure.signal_toggled().connect( sigc::mem_fun(*this, &App::on_checkbox_secure_toggled) );
	m_CheckButton_Test.signal_toggled().connect( sigc::mem_fun(*this, &App::on_checkbox_test_toggled) );
	m_Button_Login.signal_clicked().connect( sigc::mem_fun(*this, &App::on_button_login) );
	m_Button_Logout.signal_clicked().connect( sigc::mem_fun(*this, &App::on_button_logout) );
	m_Button_PauseResume.signal_clicked().connect( sigc::mem_fun(*this, &App::on_button_pause_resume) );
	m_Button_LearnSave.signal_clicked().connect( sigc::mem_fun(*this, &App::on_button_learn_save) );

	m_Entry_Password.set_visibility(false);
	m_Entry_Password.set_input_purpose(Gtk::INPUT_PURPOSE_PASSWORD);

	m_Entry_Password.signal_changed().connect( sigc::mem_fun(*this, &App::on_entry_id_password_changed) );
	m_Entry_Password.signal_activate().connect( sigc::mem_fun(*this, &App::on_entry_id_pass_entered) );
	m_Entry_Id.signal_changed().connect( sigc::mem_fun(*this, &App::on_entry_id_password_changed) );
	m_Entry_Id.signal_activate().connect( sigc::mem_fun(*this, &App::on_entry_id_pass_entered) );
	m_Entry_Name.signal_changed().connect( sigc::mem_fun(*this, &App::on_entry_name_changed) );

	m_CheckButton_Secure.set_active(true);
	m_CheckButton_Test.set_active(false);
	m_Button_Login.set_sensitive(false);
	m_Button_Logout.set_sensitive(false);
	m_Button_PauseResume.set_sensitive(false);
	m_Button_LearnSave.set_sensitive(false);
	m_CheckButton_Secure.set_sensitive(false);
	m_CheckButton_Test.set_sensitive(false);
	m_Entry_Name.set_sensitive(false);
	m_Label_Name.set_sensitive(false);

	m_Entry_Id.set_can_default();
	m_Entry_Id.grab_default();

	m_VBox_Right.pack_start(m_Image);
	m_Image.set_size_request(640, 480);

	show_all_children();
}

App::~App()
{
	g_free(this->remote_ip);
	LOG_INFO("exit app");
}

void App::show_dialog(const gchar* contents)
{
	m_pDialog.reset(new Gtk::MessageDialog(*this, contents, false, Gtk::MessageType::MESSAGE_ERROR, Gtk::ButtonsType::BUTTONS_OK, true));
	m_pDialog->signal_response().connect(sigc::hide(sigc::mem_fun(*m_pDialog, &Gtk::Widget::hide)));
	m_pDialog->show();
}

gboolean App::connect_server ()
{
	gboolean ret = FALSE;
	guint8 res = 255;
	const gchar *ca;
	const gchar *crt;
	const gchar *key;

	if (0 != check_client_cert()) {
		LOG_WARNING("Certificate Error");
		show_dialog("Certificate Error");
		goto exit;
	}
	else {
		ca = getFilepath_ca_cert();
		crt = getFilepath_client_cert();
		key = getFilepath_client_key();
	}

	if (this->port_control != NULL) {
		LOG_WARNING("this.port_control is not NULL");
		goto exit;
	}

	if (this->port_secure != NULL) {
		LOG_WARNING("this.port_secure is not NULL");
		goto exit;
	}

	if (this->port_nonsecure != NULL) {
		LOG_WARNING("this.port_nonsecure is not NULL");
		goto exit;
	}

	if (this->port_meta != NULL) {
		LOG_WARNING("this.port_meta is not NULL");
		goto exit;
	}

	if ((this->port_control = OpenTcpConnection(this->remote_ip, "5000", ca, crt, key)) == NULL) // Open TCP TLS port for control data
	{
		LOG_WARNING("error open port_control");
		show_dialog("Connection Fail");
		goto exit;
	}

	// send login info and check
	TcpSendLoginData(this->port_control, m_Entry_Id.get_text().c_str(), m_Entry_Password.get_text().c_str());
	show_dialog("Show your face on camera within 5sec");

	g_test_timer_start ();
	while(5.5f >= g_test_timer_elapsed ()) {
		waitKey(300);
		if (readysocket(SSL_get_fd(this->port_control->ssl))) {
			gssize recv_ret = 0;
			recv_ret = TcpRecvRes(this->port_control, &res);

			if (recv_ret == TCP_RECV_TIMEOUT) {
				continue;
			}

			if (recv_ret == TCP_RECV_PEER_DISCONNECTED || recv_ret == TCP_RECV_ERROR || res == RES_OK) {
				break;
			}
		}
	}
	g_test_timer_last ();

	m_pDialog->hide();

	if (res != RES_OK) {
		LOG_WARNING("login fail");
		disconnect_server();
		show_dialog("Login Fail");
		goto exit;
	}

	if ((this->port_secure = OpenTcpConnection(this->remote_ip, "5000", ca, crt, key)) == NULL)	// Open TCP TLS port for secure mode
	{
		LOG_WARNING("error open port_secure");
		disconnect_server();
		show_dialog("Connection Fail");
		goto exit;
	}

	if ((this->port_nonsecure = OpenTcpConnection(this->remote_ip, "5000", NULL, NULL, NULL)) == NULL) // Open TCP TLS port for non_secure mode
	{
		LOG_WARNING("error open port_nonsecure");
		disconnect_server();
		show_dialog("Connection Fail");
		goto exit;
	}

	if ((this->port_meta = OpenTcpConnection(this->remote_ip, "5000", ca, crt, key)) == NULL) // Open TCP TLS port for meta data
	{
		LOG_WARNING("error open port_meta");
		disconnect_server();
		show_dialog("Connection Fail");
		goto exit;
	}

	this->connected_server = true;
	this->port_recv_photo = this->port_secure;
	this->handle_recv_data = g_idle_add(handle_port_secure, this);
	ret = TRUE;

exit:
	return ret;
}

gboolean App::disconnect_server ()
{
	this->connected_server = false;

	if (this->port_control != NULL) {
		CloseTcpConnectedPort(&this->port_control); // Close network port;
		this->port_control = NULL;
	}

	if (this->port_secure != NULL) {
		CloseTcpConnectedPort(&this->port_secure); // Close network port;
		this->port_secure = NULL;
	}

	if (this->port_nonsecure != NULL) {
		CloseTcpConnectedPort(&this->port_nonsecure); // Close network port;
		this->port_nonsecure = NULL;
	}

	if (this->port_meta != NULL) {
		CloseTcpConnectedPort(&this->port_meta); // Close network port;
		this->port_meta = NULL;
	}

	if(this->handle_recv_data) {
		g_source_remove(this->handle_recv_data);
		this->handle_recv_data = 0;
	}

	this->port_recv_photo = NULL;

	return TRUE;
}

CLIENT_STATE App::get_current_app_state()
{
	if (this->connected_server) {
		if (this->learn_mode_state != LEARN_NONE) {
			return CLIENT_STATE_LEARN;
		}
		else if (m_CheckButton_Secure.get_active()) {
			if (m_CheckButton_Test.get_active()) {
				return CLIENT_STATE_SECURE_TESTRUN;
			}
			else {
				return CLIENT_STATE_SECURE_RUN;
			}
		}
		else { // non secure
			if (m_CheckButton_Test.get_active()) {
				return CLIENT_STATE_NONSECURE_TESTRUN;
			}
			else {
				return CLIENT_STATE_NONSECURE_RUN;
			}
		}
	}
	else
	{
		return CLIENT_STATE_LOGOUT;
	}
}

gboolean App::check_valid_input (const gchar *regex_str, const gchar *target_str)
{
	gboolean ret = FALSE;
	GError *err = NULL;
	GMatchInfo *match_info = NULL;
	GRegex *regex;

	regex = g_regex_new (regex_str, (GRegexCompileFlags)0, (GRegexMatchFlags)0, &err);
	if (regex == NULL) {
		LOG_WARNING ("regex error!");
		if (err != NULL) {
			LOG_WARNING ("error %s!", err->message);
			g_clear_error(&err);
		}

		goto exit;
	}

	g_regex_match (regex, target_str, (GRegexMatchFlags)0, &match_info);

	if (g_match_info_matches (match_info)) {
		ret = TRUE;
	}

	g_match_info_free (match_info);
	g_regex_unref (regex);

exit:
	return ret;
}

void App::on_button_login()
{
	if (check_valid_input("^[a-zA-Z0-9]+$", m_Entry_Id.get_text().c_str()) &&
		check_valid_input("^(?=.*[A-Za-z])(?=.*\\d)(?=.*[\\^@$!%*#?&])[A-Za-z\\d\\^@$!%*#?&]{8,}$", m_Entry_Password.get_text().c_str())) {
		// id: alphabet and number
		// pass: Minimum eight characters, at least one letter, one number and one special character
		if (this->connect_server()) {
			/* disables */
			m_Entry_Id.set_sensitive(false);
			m_Entry_Password.set_sensitive(false);
			m_Button_Login.set_sensitive(false);
			m_Button_LearnSave.set_sensitive(false);
			m_Label_Login.set_sensitive(false);
			m_Label_Password.set_sensitive(false);
			m_Entry_Name.set_sensitive(false);
			m_Label_Name.set_sensitive(false);

			/* enables */
			m_Button_Logout.set_sensitive(true);
			m_Button_PauseResume.set_sensitive(true);
			m_CheckButton_Secure.set_sensitive(true);
			m_CheckButton_Test.set_sensitive(true);
		}
		else {
			m_Entry_Id.grab_focus();
			m_Entry_Id.set_text("");
			m_Entry_Password.set_text("");
			m_Entry_Name.set_text("");
		}
	}
	else {
		m_Entry_Id.grab_focus();
		m_Entry_Id.set_text("");
		m_Entry_Password.set_text("");
		m_Button_Login.set_sensitive(false);
		LOG_WARNING("login syntax");
		show_dialog("Login Fail");
	}
}

void App::on_button_logout()
{
	guint8 res = 255;

	if (this->port_control != NULL) {
		gssize recv_ret = 0;
		TcpSendLogoutReq(this->port_control);
		recv_ret = TcpRecvRes(this->port_control, &res);

		if ((recv_ret == TCP_RECV_PEER_DISCONNECTED || recv_ret == TCP_RECV_ERROR || recv_ret == TCP_RECV_TIMEOUT) ||
				res != RES_OK) {
			LOG_WARNING("TcpRecvRes fail");
		}
	}

	this->disconnect_server();

	/* disables */
	m_Button_Login.set_sensitive(false);
	m_Button_Logout.set_sensitive(false);
	m_Button_PauseResume.set_sensitive(false);
	m_Button_LearnSave.set_sensitive(false);
	m_CheckButton_Secure.set_sensitive(false);
	m_CheckButton_Test.set_sensitive(false);
	m_Entry_Name.set_sensitive(false);
	m_Label_Name.set_sensitive(false);

	/* enables */
	m_Entry_Id.set_sensitive(true);
	m_Entry_Password.set_sensitive(true);
	m_Label_Login.set_sensitive(true);
	m_Label_Password.set_sensitive(true);

	/* setting values */
	m_Entry_Id.grab_focus();
	m_Entry_Id.set_text("");
	m_Entry_Password.set_text("");
	m_Entry_Name.set_text("");
	m_CheckButton_Secure.set_active(true);
	m_CheckButton_Test.set_active(false);
	m_Button_PauseResume.set_label("Pause");

	this->learn_mode_state = LEARN_NONE;
}

void App::on_checkbox_secure_toggled()
{
	guint8 res = 255;

	if (this->connected_server) {
		gssize recv_ret = 0;
		if (m_CheckButton_Secure.get_active()) {
			// send secure mode
			TcpSendSecureModeReq(this->port_control);
			recv_ret = TcpRecvRes(this->port_control, &res);

			if ((recv_ret == TCP_RECV_PEER_DISCONNECTED || recv_ret == TCP_RECV_ERROR || recv_ret == TCP_RECV_TIMEOUT) ||
					res != RES_OK) {
				LOG_WARNING("TcpRecvRes fail");
				disconnect_server();
				show_dialog("fail request. error no: 1");
			}
			else {
				this->port_recv_photo = this->port_secure;
			}
		}
		else {
			// send non secure mode
			TcpSendNonSecureModeReq(this->port_control);
			recv_ret = TcpRecvRes(this->port_control, &res);

			if ((recv_ret == TCP_RECV_PEER_DISCONNECTED || recv_ret == TCP_RECV_ERROR || recv_ret == TCP_RECV_TIMEOUT) ||
					res != RES_OK) {
				LOG_WARNING("TcpRecvRes fail");
				disconnect_server();
				show_dialog("fail request. error no: 2");
			}
			else {
				this->port_recv_photo = this->port_nonsecure;
			}
		}
	}
}

void App::on_checkbox_test_toggled()
{
	guint8 res = 255;

	if (this->connected_server) {
		gssize recv_ret = 0;
		if (m_CheckButton_Test.get_active()) {
			// send test mode
			TcpSendTestRunModeReq(this->port_control);
			recv_ret = TcpRecvRes(this->port_control, &res);

			if ((recv_ret == TCP_RECV_PEER_DISCONNECTED || recv_ret == TCP_RECV_ERROR || recv_ret == TCP_RECV_TIMEOUT) ||
					res != RES_OK) {
				LOG_WARNING("TcpRecvRes fail");
				disconnect_server();
				show_dialog("fail request. error no: 3");
			}
		}
		else {
			// send run mode
			TcpSendRunModeReq(this->port_control);
			recv_ret = TcpRecvRes(this->port_control, &res);

			if ((recv_ret == TCP_RECV_PEER_DISCONNECTED || recv_ret == TCP_RECV_ERROR || recv_ret == TCP_RECV_TIMEOUT) ||
					res != RES_OK) {
				LOG_WARNING("TcpRecvRes fail");
				disconnect_server();
				show_dialog("fail request. error no: 4");
			}
		}
	}
}

void App::on_button_pause_resume()
{
	guint8 res = 255;

	if (this->connected_server) {
		gssize recv_ret = 0;
		if (this->learn_mode_state == LEARN_NONE) {
			// paused
			this->learn_mode_state = LEARN_REQUESTED;
			m_Button_PauseResume.set_label("Resume");
			m_CheckButton_Secure.set_sensitive(false);
			m_CheckButton_Test.set_sensitive(false);
			TcpSendCaptureReq(this->port_control);
			recv_ret = TcpRecvRes(this->port_control, &res);
			LOG_WARNING("get resp");

			if ((recv_ret == TCP_RECV_PEER_DISCONNECTED || recv_ret == TCP_RECV_ERROR || recv_ret == TCP_RECV_TIMEOUT) ||
					res != RES_OK) {
				LOG_WARNING("TcpRecvRes fail");
				disconnect_server();
				show_dialog("fail request. error no: 5");
			}
		}
		else {
			// play = resume
			this->learn_mode_state = LEARN_NONE;
			m_Entry_Name.set_text("");
			m_Entry_Name.set_sensitive(false);
			m_Label_Name.set_sensitive(false);
			m_CheckButton_Secure.set_sensitive(true);
			m_CheckButton_Test.set_sensitive(true);
			m_Button_PauseResume.set_label("Pause");
			if (m_CheckButton_Secure.get_active()) {
				// send secure mode
				this->port_recv_photo = this->port_secure;
				TcpSendSecureModeReq(this->port_control);
				recv_ret = TcpRecvRes(this->port_control, &res);

				if ((recv_ret == TCP_RECV_PEER_DISCONNECTED || recv_ret == TCP_RECV_ERROR || recv_ret == TCP_RECV_TIMEOUT) ||
						res != RES_OK) {
					LOG_WARNING("TcpRecvRes fail");
					disconnect_server();
					show_dialog("fail request. error no: 6");
				}
			}
			else {
				// send non secure mode
				this->port_recv_photo = this->port_nonsecure;
				TcpSendNonSecureModeReq(this->port_control);
				recv_ret = TcpRecvRes(this->port_control, &res);

				if ((recv_ret == TCP_RECV_PEER_DISCONNECTED || recv_ret == TCP_RECV_ERROR || recv_ret == TCP_RECV_TIMEOUT) ||
						res != RES_OK) {
					LOG_WARNING("TcpRecvRes fail");
					disconnect_server();
					show_dialog("fail request. error no: 7");
				}
			}
		}
	}
}

void App::on_button_learn_save()
{
	guint8 res = 255;

	if (this->connected_server) {
		if (check_valid_input("^[a-zA-Z0-9 ,._'`-]+$", m_Entry_Name.get_text().c_str()) &&
			0 != g_ascii_strncasecmp(m_Entry_Name.get_text().c_str(), "admin", sizeof("admin"))) {
			gssize recv_ret = 0;
			TcpSendSaveReq(this->port_control, m_Entry_Name.get_text().c_str());
			recv_ret = TcpRecvRes(this->port_control, &res);

			if ((recv_ret == TCP_RECV_PEER_DISCONNECTED || recv_ret == TCP_RECV_ERROR || recv_ret == TCP_RECV_TIMEOUT) ||
					res != RES_OK) {
				LOG_WARNING("TcpRecvRes fail");
				disconnect_server();
				show_dialog("fail request. error no: 8");
			}
			else {
				this->on_button_pause_resume(); // resume again
				this->show_dialog("save done");
			}
		}
		else {
			this->show_dialog("Name is only allowed\nalphabet, number, and ,._'`- character only\nalso not allowed 'admin'");
		}
	}
}

void App::on_entry_name_changed()
{
	if (m_Entry_Name.get_text().length() != 0) {
		m_Button_LearnSave.set_sensitive(true);
	}
	else {
		m_Button_LearnSave.set_sensitive(false);
	}
}

void App::on_entry_id_password_changed()
{
	if (m_Entry_Id.get_text().length() != 0 && m_Entry_Password.get_text().length() != 0) {
		m_Button_Login.set_sensitive(true);
	}
	else {
		m_Button_Login.set_sensitive(false);
	}
}

void App::on_entry_id_pass_entered()
{
	if (m_Entry_Id.get_text().length() != 0 && m_Entry_Password.get_text().length() != 0) {
		this->on_button_login();
	}
}

bool App::on_delete_event(GdkEventAny* any_event)
{
	// when app closing
	this->disconnect_server();
	return Gtk::Window::on_delete_event(any_event);
}
