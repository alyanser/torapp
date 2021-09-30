#include "main_window.hxx"

#include <QApplication>

int main(int argc,char ** argv){
         QApplication application(argc,argv);
         Main_window window;
	
	window.showMaximized();
         QObject::connect(&window,&Main_window::quit,&application,&QApplication::quit);
	
         return QApplication::exec();
}