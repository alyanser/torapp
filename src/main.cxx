#include "main_window.hxx"

#include <QApplication>
#include <QFile>

int main(int argc,char ** argv){
         QApplication::setOrganizationName("tast");
         QApplication::setApplicationName("torapp");

         QApplication torapp(argc,argv);

         torapp.setWindowIcon(QIcon(":app_icon.png"));

         torapp.setStyleSheet([]{
                  QFile stylesheet_file(":app_stylesheet.qss");
                  return stylesheet_file.open(QFile::ReadOnly) ? stylesheet_file.readAll() : "";
         }());

         Main_window main_window;
         QObject::connect(&main_window,&Main_window::closed,&torapp,&QApplication::quit);
         
         return QApplication::exec();
}