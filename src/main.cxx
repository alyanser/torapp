#include "main_window.hxx"

#include <QApplication>
#include <QFile>

int main(int argc,char ** argv){
         QApplication torapp(argc,argv);

         QApplication::setOrganizationName("conat");
         QApplication::setApplicationName("torapp");
         QApplication::setWindowIcon(QIcon(":app_icon.png"));

         if(QFile stylesheet_file(":app_stylesheet.qss");stylesheet_file.open(QFile::ReadOnly)){
                  torapp.setStyleSheet(stylesheet_file.readAll());
         }

         Main_window main_window;
         QObject::connect(&main_window,&Main_window::closed,&torapp,&QApplication::quit);
         
         return QApplication::exec();
}