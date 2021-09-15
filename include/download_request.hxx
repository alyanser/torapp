#ifndef DOWNLOAD_REQUEST_HXX
#define DOWNLOAD_REQUEST_HXX

struct Download_request {
         QString package_name;
         QString download_path;
         QUrl url;
};

#endif // DOWNLOAD_REQUEST_HXX