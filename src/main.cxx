#include "main_window.hxx"

#include <QApplication>
#include <QFile>

int main(int argc,char ** argv){
         QApplication app(argc,argv);

         QApplication::setOrganizationName("Tast");
         QApplication::setApplicationName("Torapp");

         {
                  QFile stylesheet_file(":app_stylesheet.qss");

                  if(stylesheet_file.open(QFile::ReadOnly)){
                           app.setStyleSheet(stylesheet_file.readAll());
                  }
         }
         
         Main_window window;
         QObject::connect(&window,&Main_window::closed,&app,&QApplication::quit);
         
         return QApplication::exec();
}