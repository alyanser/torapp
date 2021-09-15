#include "main_window.hxx"
#include <QApplication>

int main(int argc,char ** argv){
         QApplication application(argc,argv);
         Main_window window;
         QObject::connect(&window,&Main_window::quit,&application,&QApplication::quit);
         window.show();
         return QApplication::exec();
}