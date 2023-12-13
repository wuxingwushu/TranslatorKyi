#pragma once
#include <string>
#include <vector>

//是否存在文件夹
bool WebDav_Directory(std::string path, std::string directory);

//查看列表
std::vector<std::string> WebDav_List(std::string path = "");

//上传
void WebDav_Upload(std::string File, std::string path = "");

//上传文件夹
void WebDav_UploadDirectory(std::string Filepath, std::string path, std::string directory);

//下载
void WebDav_Download(std::string File, std::string path = "./");

//下载文件夹
void WebDav_DownloadDirectory(std::string directory, std::string path = "./");

//删除
void WebDav_Delete(std::string File);

//创建文件夹
void WebDav_CreateFolder(std::string File);