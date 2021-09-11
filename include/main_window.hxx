#ifndef MAIN_WINDOW_HXX
#define MAIN_WINDOW_HXX

#include "custom_url_widget.hxx"
#include <QMainWindow>
#include <QMessageBox>
#include <string_view>

class Main_window : public QMainWindow {
         Q_OBJECT
public:
         Main_window();
         Main_window(const Main_window & rhs) = delete;
         Main_window(Main_window && rhs) = delete;
         Main_window & operator = (Main_window && rhs) = delete;
         Main_window & operator = (const Main_window & rhs) = delete;
         ~Main_window() override = default;

private:
         void setup_menu_bar(QMenuBar * menu_bar) const noexcept;
         void setup_file_menu(QMenu * file_menu) const noexcept;
         void confirm_quit() const noexcept;
         void input_custom_link() noexcept;

         QWidget central_widget_;
         Custom_url_widget custom_url_widget_;

signals:
         void quit() const;
         
public slots:
         void handle_custom_url(const QUrl & custom_url) const noexcept;
};

inline Main_window::Main_window(){
         setWindowTitle("Torapp");
         setMinimumSize(QSize(800,640));
         setCentralWidget(&central_widget_);
         setup_menu_bar(menuBar());

         connect(&custom_url_widget_,&Custom_url_widget::url_received,this,&Main_window::handle_custom_url);
}

inline void Main_window::confirm_quit() const noexcept {
         constexpr std::string_view box_title("Quit");
         constexpr std::string_view termination_warning("Are you sure you want to quit? All of your downloads will be terminated.");

         const auto response = QMessageBox::question(nullptr,box_title.data(),termination_warning.data());

         if(response == QMessageBox::Yes){
                  emit quit();
         }
}

inline void Main_window::handle_custom_url(const QUrl & custom_url) const noexcept {
         qInfo() << custom_url;
}

#endif // MAIN_WINDOW_HXX