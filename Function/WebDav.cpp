#include "WebDav.h"
#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include <sstream>
#include "../Tool/rapidxml.hpp"
#include "../Variable.h"
#include <filesystem>
#include "../Tool/Tool.h"


// 回调函数，用于接收服务器响应数据
size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

// 回调函数用于处理下载的数据
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::ofstream* file = static_cast<std::ofstream*>(userp);
    file->write(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

size_t WriteCallback2(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

//获取文件名
std::string GetStrName(std::string name, const char C) {
    for (int i = name.size() - 1; i >= 0; i--)
    {
        if (name[i] == C) {
            return name.substr(i + 1, name.size() - i - 1);
        }
    }
    return name;
}

// 解码URL编码的字符串
std::string urlDecode(const std::string& encodedStr) {
    std::ostringstream decoded;
    std::istringstream input(encodedStr);

    char ch;
    int hexChar;

    while (input.get(ch)) {
        if (ch == '%') {
            if (input >> std::hex >> hexChar) {
                decoded << static_cast<char>(hexChar);
            }
            else {
                break;
            }
        }
        else {
            decoded << ch;
        }
    }

    return decoded.str();
}

//是否存在文件夹
bool WebDav_Directory(std::string path, std::string directory) {
    std::vector<std::string> List = WebDav_List(path);
    bool RepeatName = false;
    for (auto i : List)
    {
        if (directory == i) {
            RepeatName = true;
            break;
        }
    }
    return RepeatName;
}


std::vector<std::string> WebDav_List(std::string path) {
    CURL* curl;
    CURLcode res;
    std::vector<std::string> List;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        // 设置WebDAV地址
        curl_easy_setopt(curl, CURLOPT_URL, (Variable::WebDav_url + path).c_str());

        // 设置用户名和密码
        curl_easy_setopt(curl, CURLOPT_USERNAME, Variable::WebDav_username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, Variable::WebDav_password.c_str());

        // 设置 HTTP method 为 PROPFIND，用于列出文件和文件夹
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");

        // 设置响应数据的写入回调函数
        std::string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback2);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // 发送HTTP请求并获取响应
        res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            // 解析XML文件
            rapidxml::xml_document<>* doc = new rapidxml::xml_document<>();
            doc->parse<0>(&response[0]);

            // 遍历每个<d:href>标签，并输出文件路径
            rapidxml::xml_node<>* root = doc->first_node("d:multistatus");
            for (rapidxml::xml_node<>* node = root->first_node("d:response"); node; node = node->next_sibling("d:response")) {
                rapidxml::xml_node<>* hrefNode = node->first_node("d:href");
                std::string T = node->first_node("d:propstat")->first_node("d:prop")->first_node("d:getcontenttype")->value();
                bool Tbool = (T == "httpd/unix-directory");

                if (hrefNode) {
                    std::string kao = urlDecode(hrefNode->value());
                    if (List.size() == 0) {
                        List.push_back(kao + (kao[kao.size() - 1] == '/' ? "" : "/"));
                    }
                    else {
                        List.push_back(kao.substr(List[0].size(), kao.size() - List[0].size()) + (Tbool ? "/" : ""));
                    }
                }
            }

            delete doc;
        }
        else {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();

    List[0] = List.back();
    List.pop_back();
    return List;
}

//上传
void WebDav_Upload(std::string File, std::string path) {
    CURL* curl;
    FILE* file;
    CURLcode res;

    // 初始化 libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // 创建一个新的 libcurl 连接
    curl = curl_easy_init();
    if (curl) {
        // 设置 WebDAV URL
        curl_easy_setopt(curl, CURLOPT_URL, Variable::WebDav_url.c_str());

        // 设置用户名和密码
        curl_easy_setopt(curl, CURLOPT_USERNAME, Variable::WebDav_username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, Variable::WebDav_password.c_str());

        // 发送请求并获取根目录下的文件和文件夹列表
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "Failed to list files: " << curl_easy_strerror(res) << std::endl;
        }

        // 上传文件
        for (size_t i = 0; i < File.size(); i++)
        {
            if (File[i] == '\\') {
                File[i] = '/';
            }
        }
        file = fopen(File.c_str(), "rb");
        File = GetStrName(File, '/');
        
        if (file) {
            // 设置要上传的本地文件路径
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
            curl_easy_setopt(curl, CURLOPT_READDATA, file);

            // 设置要上传到的远程文件的完整URI，包括沙盒标题
            std::string full_remote_url = Variable::WebDav_url + Variable::WebDav_WebFile + "/" + path + "/" + File;
            curl_easy_setopt(curl, CURLOPT_URL, full_remote_url.c_str());

            // 执行上传操作
            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "Failed to upload file: " << curl_easy_strerror(res) << std::endl;
            }

            // 关闭本地文件
            fclose(file);
        }
    }

    // 清理 libcurl 资源
    curl_easy_cleanup(curl);
    curl_global_cleanup();
}

void WebDav_UploadDirectory(std::string Filepath, std::string path, std::string directory) {
    WIN32_FIND_DATA findFileData;
    
    HANDLE hFind = FindFirstFile((Filepath + "*").c_str(), &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::cout << "Error finding files in directory!" << std::endl;
    }

    if (FindNextFile(hFind, &findFileData) != 0) {
        if (!WebDav_Directory(Variable::WebDav_WebFile + "/" + path, directory)) {
            WebDav_CreateFolder(path + "/" + directory);
        }
    }

    do {
        std::string name = findFileData.cFileName;
        if ((name == "..") || (name == ".")) {
            continue;
        }
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {//文件夹
            WebDav_UploadDirectory(Filepath + findFileData.cFileName + "\\", path + "/" + directory, findFileData.cFileName);
        }
        else {//文件
            WebDav_Upload(Filepath + findFileData.cFileName, path + "/" + directory);
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);
}

void WebDav_Download(std::string File, std::string path) {
    // 初始化libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // 创建CURL对象
    CURL* curl = curl_easy_init();
    if (curl) {
        // 下载文件
        std::string local_file_path = path + GetStrName(File, '/');
        std::string full_remote_url = Variable::WebDav_url + Variable::WebDav_WebFile + "/" + File;
        // 设置WebDAV地址
        curl_easy_setopt(curl, CURLOPT_URL, full_remote_url.c_str());

        // 设置用户名和密码
        curl_easy_setopt(curl, CURLOPT_USERNAME, Variable::WebDav_username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, Variable::WebDav_password.c_str());

        // 打开本地文件
        std::ofstream file(local_file_path, std::ios::binary);

        // 设置回调函数
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

        // 执行请求
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "Failed to download file: " << curl_easy_strerror(res) << std::endl;
        }

        // 关闭文件
        file.close();

        // 清理CURL对象
        curl_easy_cleanup(curl);
    }

    // 清理libcurl
    curl_global_cleanup();
}

void WebDav_DownloadDirectory(std::string directory, std::string path) {
    if (std::filesystem::exists(path)) {
        std::cout << "Folder already exists." << std::endl;
    }
    else {
        if (std::filesystem::create_directory(path)) {
            std::cout << "Folder created successfully." << std::endl;
        }
        else {
            std::cout << "Failed to create folder." << std::endl;
        }
    }

    std::vector<std::string> List = WebDav_List(Variable::WebDav_WebFile + "/" + directory);
    for (size_t i = 0; i < List.size(); i++)
    {
        if (List[i][List[i].size() - 1] == '/') {
            WebDav_DownloadDirectory(directory + List[i], path + List[i]);
        }
        else {
            WebDav_Download(List[i], path);
        }
    }
}


void WebDav_Delete(std::string File) {
    CURL* curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        // 设置 WebDAV 地址
        std::string full_remote_url = Variable::WebDav_url + Variable::WebDav_WebFile + "/" + File;
        curl_easy_setopt(curl, CURLOPT_URL, full_remote_url.c_str());

        // 设置用户名和密码
        curl_easy_setopt(curl, CURLOPT_USERNAME, Variable::WebDav_username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, Variable::WebDav_password.c_str());

        // 使用 HTTP DELETE 方法删除文件
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

        // 执行请求
        res = curl_easy_perform(curl);

        // 检查请求执行结果
        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));

        // 清理资源
        curl_easy_cleanup(curl);
    }
}

void WebDav_CreateFolder    (std::string File) {
    // 初始化 curl
    CURL* curl = curl_easy_init();

    // 设置 WebDAV 地址
    curl_easy_setopt(curl, CURLOPT_URL, Variable::WebDav_url.c_str());

    // 设置用户名和密码
    curl_easy_setopt(curl, CURLOPT_USERNAME, Variable::WebDav_username.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, Variable::WebDav_password.c_str());

    // 设置为 PUT 请求
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    // 设置要上传的内容为空，表示创建一个空文件夹
    curl_easy_setopt(curl, CURLOPT_READDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE, 0L);

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "MKCOL");

    // 设置要创建的文件夹名称
    std::string folder_url = Variable::WebDav_url + Variable::WebDav_WebFile + "/" + File;
    curl_easy_setopt(curl, CURLOPT_URL, folder_url.c_str());

    // 执行请求
    CURLcode res = curl_easy_perform(curl);

    // 检查是否成功
    if (res != CURLE_OK) {
        std::cerr << "Error: " << curl_easy_strerror(res) << std::endl;
    }

    // 清理 curl
    curl_easy_cleanup(curl);
}