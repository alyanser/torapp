#include "main_window.hxx"

#include <QApplication>
#include <QFile>

int main(int argc,char ** argv){
         QApplication::setOrganizationName("Tast");
         QApplication::setApplicationName("Torapp");

         QApplication torapp(argc,argv);

         torapp.setStyleSheet([]{
                  QFile stylesheet_file(":app_stylesheet.qss");
                  return stylesheet_file.open(QFile::ReadOnly) ? stylesheet_file.readAll() : "";
         }());

         Main_window main_window;
         QObject::connect(&main_window,&Main_window::closed,&torapp,&QApplication::quit);
         
         return QApplication::exec();
}