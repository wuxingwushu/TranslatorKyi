#include "WebDav.h"
#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include <sstream>
#include "../Tool/rapidxml.hpp"
#include "../Variable.h"
#include <filesystem>
#include "../Tool/Tool.h"
#include "../Tool/Http.h"    //TOOL::HttpStringSink / TOOL::HttpFileSink / TOOL::MakeCurl
#include "../Tool/UrlCodec.h"//TOOL::UrlDecode


// 写回调已收敛到 TOOL::HttpStringSink(→std::string*) / TOOL::HttpFileSink(→FILE*)；
// 获取文件名收敛到 TOOL::BaseName、URL 解码收敛到 TOOL::UrlDecode。

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

//查看列表
std::vector<std::string> WebDav_List(std::string path) {
    CURLcode res;
    std::vector<std::string> List;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    TOOL::CurlPtr curl = TOOL::MakeCurl();
    if (curl) {
        // 设置WebDAV地址
        curl_easy_setopt(curl.get(), CURLOPT_URL, (Variable::WebDav_url + path).c_str());

        // 设置用户名和密码
        curl_easy_setopt(curl.get(), CURLOPT_USERNAME, Variable::WebDav_username.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_PASSWORD, Variable::WebDav_password.c_str());

        // 设置 HTTP method 为 PROPFIND，用于列出文件和文件夹
        curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "PROPFIND");

        // 设置响应数据的写入回调函数
        std::string response;
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, TOOL::HttpStringSink);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);

        // 发送HTTP请求并获取响应
        res = curl_easy_perform(curl.get());

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
                    std::string kao = TOOL::UrlDecode(hrefNode->value());
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
    }
    curl_global_cleanup();

    if (List.size() >= 2) {
        List[0] = List.back();
        List.pop_back();
    }
    return List;
}

//上传
void WebDav_Upload(std::string File, std::string path) {
    FILE* file;
    CURLcode res;

    // 初始化 libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // 创建一个新的 libcurl 连接
    TOOL::CurlPtr curl = TOOL::MakeCurl();
    if (curl) {
        // 设置 WebDAV URL
        curl_easy_setopt(curl.get(), CURLOPT_URL, Variable::WebDav_url.c_str());

        // 设置用户名和密码
        curl_easy_setopt(curl.get(), CURLOPT_USERNAME, Variable::WebDav_username.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_PASSWORD, Variable::WebDav_password.c_str());

        // 发送请求并获取根目录下的文件和文件夹列表
        res = curl_easy_perform(curl.get());
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
        File = TOOL::BaseName(File);
        
        if (file) {
            // 设置要上传的本地文件路径
            curl_easy_setopt(curl.get(), CURLOPT_UPLOAD, 1);
            curl_easy_setopt(curl.get(), CURLOPT_READDATA, file);

            // 设置要上传到的远程文件的完整URI，包括沙盒标题
            std::string full_remote_url = Variable::WebDav_url + Variable::WebDav_WebFile + "/" + path + "/" + File;
            curl_easy_setopt(curl.get(), CURLOPT_URL, full_remote_url.c_str());

            // 执行上传操作
            res = curl_easy_perform(curl.get());
            if (res != CURLE_OK) {
                std::cerr << "Failed to upload file: " << curl_easy_strerror(res) << std::endl;
            }

            // 关闭本地文件
            fclose(file);
        }
    }

    curl_global_cleanup();
}

//上传文件夹
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

//下载
void WebDav_Download(std::string File, std::string path) {
    // 初始化libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // 创建CURL对象
    TOOL::CurlPtr curl = TOOL::MakeCurl();
    if (curl) {
        // 下载文件
        std::string local_file_path = path + TOOL::BaseName(File);
        std::string full_remote_url = Variable::WebDav_url + Variable::WebDav_WebFile + "/" + File;
        // 设置WebDAV地址
        curl_easy_setopt(curl.get(), CURLOPT_URL, full_remote_url.c_str());

        // 设置用户名和密码
        curl_easy_setopt(curl.get(), CURLOPT_USERNAME, Variable::WebDav_username.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_PASSWORD, Variable::WebDav_password.c_str());

        // 打开本地文件（非 ASCII 路径按 UTF-8 转宽字符再开）
        FILE* file = TOOL::OpenUtf8File(local_file_path, L"wb");

        // 设置回调函数
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, TOOL::HttpFileSink);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, file);

        // 执行请求
        CURLcode res = curl_easy_perform(curl.get());
        if (res != CURLE_OK) {
            std::cerr << "Failed to download file: " << curl_easy_strerror(res) << std::endl;
        }

        // 关闭文件
        if (file != nullptr) {
            fclose(file);
        }
    }

    // 清理libcurl
    curl_global_cleanup();
}

//下载文件夹
void WebDav_DownloadDirectory(std::string directory, std::string path) {
    if (std::filesystem::exists(path)) {//判断是否存在文件夹
        std::cout << "Folder already exists." << std::endl;
    }
    else {
        if (std::filesystem::create_directory(path)) {//创建文件夹
            std::cout << "Folder created successfully." << std::endl;
        }
        else {
            std::cout << "Failed to create folder." << std::endl;
        }
    }

    std::vector<std::string> List = WebDav_List(Variable::WebDav_WebFile + "/" + directory);//获取列表
    for (size_t i = 0; i < List.size(); i++)
    {
        if (List[i][List[i].size() - 1] == '/') {
            WebDav_DownloadDirectory(directory + List[i], path + List[i]);//下载文件夹
        }
        else {
            WebDav_Download(List[i], path);//下载文件
        }
    }
}

//删除
void WebDav_Delete(std::string File) {
    CURLcode res;

    TOOL::CurlPtr curl = TOOL::MakeCurl();
    if (curl) {
        // 设置 WebDAV 地址
        std::string full_remote_url = Variable::WebDav_url + Variable::WebDav_WebFile + "/" + File;
        curl_easy_setopt(curl.get(), CURLOPT_URL, full_remote_url.c_str());

        // 设置用户名和密码
        curl_easy_setopt(curl.get(), CURLOPT_USERNAME, Variable::WebDav_username.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_PASSWORD, Variable::WebDav_password.c_str());

        // 使用 HTTP DELETE 方法删除文件
        curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "DELETE");

        // 执行请求
        res = curl_easy_perform(curl.get());

        // 检查请求执行结果
        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
    }
}

//创建文件夹
void WebDav_CreateFolder    (std::string File) {
    // 初始化 curl
    TOOL::CurlPtr curl = TOOL::MakeCurl();

    if (curl) {
        // 设置 WebDAV 地址
        curl_easy_setopt(curl.get(), CURLOPT_URL, Variable::WebDav_url.c_str());

        // 设置用户名和密码
        curl_easy_setopt(curl.get(), CURLOPT_USERNAME, Variable::WebDav_username.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_PASSWORD, Variable::WebDav_password.c_str());

        // 设置为 PUT 请求
        curl_easy_setopt(curl.get(), CURLOPT_UPLOAD, 1L);

        // 设置要上传的内容为空，表示创建一个空文件夹
        curl_easy_setopt(curl.get(), CURLOPT_READDATA, NULL);
        curl_easy_setopt(curl.get(), CURLOPT_INFILESIZE, 0L);

        curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "MKCOL");

        // 设置要创建的文件夹名称
        std::string folder_url = Variable::WebDav_url + Variable::WebDav_WebFile + "/" + File;
        curl_easy_setopt(curl.get(), CURLOPT_URL, folder_url.c_str());

        // 执行请求
        CURLcode res = curl_easy_perform(curl.get());

        // 检查是否成功
        if (res != CURLE_OK) {
            std::cerr << "Error: " << curl_easy_strerror(res) << std::endl;
        }
    }
}