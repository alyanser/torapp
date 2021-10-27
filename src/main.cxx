#include "main_window.hxx"

#include <QApplication>

int main(int argc,char ** argv){
         QApplication app(argc,argv);

         QApplication::setOrganizationName("Tast");
         QApplication::setApplicationName("Torapp");
         
         Main_window window;
         QObject::connect(&window,&Main_window::closed,&app,&QApplication::quit);
         
         return QApplication::exec();
}