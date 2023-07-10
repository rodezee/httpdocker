#ifdef MGOVERRIDE
    #define mg_read_httpd_file(x) mg_override_read_httpd_file(x)
#endif