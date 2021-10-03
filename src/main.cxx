#include "main_window.hxx"

#include <QApplication>

int main(int argc,char ** argv){
         QApplication app(argc,argv);
	Main_window window;
	window.showMaximized();
         return QApplication::exec();
}