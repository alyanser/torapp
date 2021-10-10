#include "main_window.hxx"

#include <QApplication>

int main(int argc,char ** argv){
         QApplication app(argc,argv);

         QApplication::setOrganizationName("Tast");
         QApplication::setApplicationName("Torapp");
         
         Main_window window;

         return QApplication::exec();
}