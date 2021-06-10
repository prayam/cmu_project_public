// File: main.cc

#include "app.h"
#include "Logger.h"
#include <gtkmm/application.h>
/*
    auto app = Gtk::Application::create(argc, argv);

    Gtk::Window window;
    window.set_default_size(600,400);

    return app->run(window);
*/

int main (int argc, char *argv[])
{
	log_enable("client");

	auto app = Gtk::Application::create(argc, argv, "org.gtkmm.example");
	App window;

	//Shows the window and returns when it is closed.
	int ret = app->run(window);

	log_disable();
	return ret;
}
