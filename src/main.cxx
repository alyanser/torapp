#include "main_window.hxx"

#include <QApplication>
#include <QFile>

int main(int argc,char ** argv){
         QApplication torapp(argc,argv);

         QApplication::setOrganizationName("conat");
         QApplication::setApplicationName("torapp");
         QApplication::setWindowIcon(QIcon(":app_icon.png"));

         if(QFile stylesheet(":app_stylesheet.qss");stylesheet.open(QFile::ReadOnly)){
                  torapp.setStyleSheet(stylesheet.readAll());
         }

         Main_window main_window;
         QObject::connect(&main_window,&Main_window::closed,&torapp,&QApplication::quit);
         
         return QApplication::exec();
}