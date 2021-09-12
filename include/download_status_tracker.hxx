#ifndef DOWNLOAD_STATUS_TRACKER_HXX
#define DOWNLOAD_STATUS_TRACKER_HXX

#include <QPushButton>

class Download_status_tracker : public QWidget {
public:
         Download_status_tracker(QWidget * parent = nullptr);
private:
};

inline Download_status_tracker::Download_status_tracker(QWidget * const parent) : QWidget(parent){
}
 
#endif // STATUS_TRACKER_HXX