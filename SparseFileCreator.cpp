/*PB级稀疏文件创建工具
*@version 3.0
*@return 文件创建成功为0，创建失败为1
*@compiler MinGW-w64 GCC 11.5.0 64-bit Release
*@author SYSTEM-RAMOS-ZDY
*@link https://space.bilibili.com/493998035
*@see 在代码里求个点赞求个关注(oﾟvﾟ)*/

#include <windows.h>
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <climits>

constexpr long long ONE_KB = 1024LL;
constexpr long long ONE_MB = ONE_KB * 1024LL;
constexpr long long ONE_GB = ONE_MB * 1024LL;
constexpr long long ONE_TB = ONE_GB * 1024LL;
constexpr long long ONE_PB = ONE_TB * 1024LL; // 1PB = 1125899906842624 字节

// 不同系统的单文件大小上限（客户端/服务器）
constexpr long long MAX_FILE_SIZE_CLIENT = 16 * ONE_TB;  // Win10/11 最大16TB
constexpr long long MAX_FILE_SIZE_SERVER = 16 * ONE_PB;   // Server 最大16PB

// 错误处理辅助函数
void PrintLastError(const std::string& operation){
	DWORD errorCode = GetLastError();
	LPSTR errorMsgBuffer = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,errorCode,MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPSTR)&errorMsgBuffer,0,nullptr);
	std::cerr << "[错误] " << operation << " 失败，错误码：" << errorCode << "，描述：" << (errorMsgBuffer ? errorMsgBuffer : "未知错误") << std::endl;
	std::cerr << "[温馨提示] 建议使用ReFS文件系统创建大文件！NTFS可能会出错，\n另外，系统最大限制为16TB/PB，实际能取到的值约为15.9999。"<< std::endl;
	std::cerr << "如果始终无法解决，请联系 SYSTEM-RAMOS-ZDY 以获得帮助。\n记得先点关注！" << std::endl;
	if (errorMsgBuffer) LocalFree(errorMsgBuffer);
}

// 格式化文件大小
std::string FormatFileSize(long long bytes){
	std::ostringstream oss;
	if (bytes >= ONE_PB)
		oss << std::fixed << std::setprecision(2) << (double)bytes / ONE_PB << " PB (" << bytes << " 字节)";
	else if (bytes >= ONE_TB)
		oss << std::fixed << std::setprecision(2) << (double)bytes / ONE_TB << " TB (" << bytes << " 字节)";
	else if (bytes >= ONE_GB)
		oss << std::fixed << std::setprecision(2) << (double)bytes / ONE_GB << " GB (" << bytes << " 字节)";
	else
		oss << bytes << " 字节";
	return oss.str();
}

// 检测系统类型（客户端/服务器）
bool IsWindowsServer(){
	OSVERSIONINFOEXW osvi;
	memset(&osvi, 0, sizeof(OSVERSIONINFOEXW));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
	if (!GetVersionExW((OSVERSIONINFOW*)&osvi)) return false;
	return (osvi.wProductType != VER_NT_WORKSTATION);
}

// 解析超大数字（有彩蛋）
bool ParseLargeNumber(const std::string& str, long long& outNum){
	try{
		outNum = std::stoll(str);
		if(outNum <= 0){
			std::cerr << "你在耍我吗？你告诉我 "<< outNum <<" 字节的文件要怎么创建？"<< std::endl;
			std::cerr << "罚你去给 ZDY 一个一键三连，哼ヾ(≧へ≦)〃"<< std::endl;
		}
		return outNum > 0;
	}
	catch (const std::invalid_argument&){
		std::cerr << "[错误] 输入的文件大小不是有效的数字！" << std::endl;
		return false;
	}
	catch (const std::out_of_range&){
		std::cerr << "[错误] 输入的文件大小超出范围！" << std::endl;
		return false;
	}
}

bool CreateSparseFile(const std::string& filePath, long long fileSizeBytes){
	long long maxSupportSize = IsWindowsServer() ? MAX_FILE_SIZE_SERVER : MAX_FILE_SIZE_CLIENT;
	if (fileSizeBytes > maxSupportSize){
		std::cerr << "[错误] 当前系统不支持创建这么大的文件！\n" << std::endl;
		std::cerr << "系统类型：" << (IsWindowsServer() ? "Windows Server（最大支持" + FormatFileSize(MAX_FILE_SIZE_SERVER) + "，而且最大值是取不到的）" 
								: "Windows 客户端（最大支持" + FormatFileSize(MAX_FILE_SIZE_CLIENT) + "，而且最大值是取不到的）\n"+
								"如需创建更大的文件，请更换至 Windows Server 操作系统。\n" +
								"或联系 SYSTEM-RAMOS-ZDY 以获得帮助。记得先点关注！") << std::endl;
		return false;
	}
	
	//转换为宽字符路径
	int wlen = MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, nullptr, 0);
	wchar_t* wFilePath = new wchar_t[wlen];
	MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, wFilePath, wlen);
	
	HANDLE hFile = CreateFileW(wFilePath,GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
	delete[] wFilePath;
	
	if (hFile == INVALID_HANDLE_VALUE){
		PrintLastError("创建/打开文件");
		return false;
	}
	
	DWORD bytesReturned = 0;
	BOOL isSparse = DeviceIoControl(hFile, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
	if (!isSparse){
		PrintLastError("标记文件为稀疏文件");
		CloseHandle(hFile);
		return false;
	}
	
	// 设置文件大小
	LARGE_INTEGER liDistanceToMove;
	// 处理边界：如果文件大小为1，减1会变成0，避免负数
	liDistanceToMove.QuadPart = (fileSizeBytes == 1) ? 0 : (fileSizeBytes - 1);
	
	LARGE_INTEGER liNewFilePointer;
	if (!SetFilePointerEx(hFile, liDistanceToMove, &liNewFilePointer, FILE_BEGIN)){
		PrintLastError("移动文件指针");
		CloseHandle(hFile);
		return false;
	}
	
	if (!SetEndOfFile(hFile)){
		PrintLastError("设置文件结束位置");
		CloseHandle(hFile);
		return false;
	}
	CloseHandle(hFile);
	
	// 验证文件属性
	wlen = MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, nullptr, 0);
	wchar_t* wFilePathVerify = new wchar_t[wlen];
	MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, wFilePathVerify, wlen);
	
	WIN32_FILE_ATTRIBUTE_DATA fileAttr;
	if (GetFileAttributesExW(wFilePathVerify, GetFileExInfoStandard, &fileAttr)){
		LARGE_INTEGER fileSize;
		fileSize.LowPart = fileAttr.nFileSizeLow;
		fileSize.HighPart = fileAttr.nFileSizeHigh;
		std::cout << "[成功] 稀疏文件创建完成！" << std::endl;
		std::cout << "文件路径：" << filePath << std::endl;
		std::cout << "文件大小：" << FormatFileSize(fileSize.QuadPart) << std::endl;
	}
	else{
		PrintLastError("验证文件属性");
		delete[] wFilePathVerify;
		return false;
	}
	
	delete[] wFilePathVerify;
	return true;
}

int main(int argc, char* argv[]){
	BOOL isAdmin = FALSE;
	PSID pAdministratorsGroup = nullptr;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup)){
		CheckTokenMembership(nullptr, pAdministratorsGroup, &isAdmin);
		FreeSid(pAdministratorsGroup);
	}
	if (!isAdmin){std::cerr << "[警告] 程序未以管理员身份运行，可能导致创建失败！[" << std::endl;}
	
	if (argc != 3){
		std::cout << "使用方法：SparseFileCreator.exe <文件路径> <文件大小(字节)>" << std::endl;
		std::cout << "系统限制：" << (IsWindowsServer() ? "Windows Server（最大支持16PB）" : "Windows 客户端（最大支持16TB）") << std::endl;
		std::cout << "示例1（创建16TB文件）：SparseFileCreator.exe E:\\16TB.img " << 16 * ONE_TB << std::endl;
		std::cout << "示例2（创建8 PB文件）：SparseFileCreator.exe E:\\8PB.img " << 8 * ONE_PB << std::endl;
		std::cout << std::endl;
		std::cout << "[PB级稀疏文件创建工具]" << std::endl;
		std::cout << "Powered by SYSTEM-RAMOS-ZDY" << std::endl;
		std::cout << "https://space.bilibili.com/493998035" << std::endl;
		return 1;
	}
	
	std::string filePath = argv[1];
	long long fileSizeBytes = 0;
	if (!ParseLargeNumber(argv[2], fileSizeBytes)) return 1;
	// 创建稀疏文件
	bool result = CreateSparseFile(filePath, fileSizeBytes);
	return result ? 0 : 1;
}

/*你把代码都翻到底了！
快去给ZDY一键三连~(oﾟvﾟ)*/
