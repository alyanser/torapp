#ifndef DOWNLOAD_REQUEST_HXX
#define DOWNLOAD_REQUEST_HXX

struct Download_request {
         QUrl url;
         QString package_name;
         QString download_path;
         //todo add more information
};

#endif // DOWNLOAD_REQUEST_HXX