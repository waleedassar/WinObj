// WinObj.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "windows.h"
#include "stdio.h"


#pragma comment(lib,"ntdll.lib")

#define STATUS_BUFFER_TOO_SMALL 0xC0000023
#define STATUS_NO_MORE_ENTRIES 0x8000001A


typedef struct _UNICODE_STRING
{
	unsigned short Length;
	unsigned short MaxLength;
	unsigned long Pad;
	wchar_t* Buffer;
}UNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
  ULONGLONG           Length;
  HANDLE          RootDirectory;
  _UNICODE_STRING* ObjectName;
  ULONGLONG           Attributes;
  PVOID           SecurityDescriptor;
  PVOID           SecurityQualityOfService;
} OBJECT_ATTRIBUTES;



extern "C"
{
	int __stdcall ZwOpenDirectoryObject(HANDLE*,int DesiredAccess,_OBJECT_ATTRIBUTES*);
	int __stdcall ZwQueryDirectoryObject(HANDLE,void* Buffer,unsigned long BufferSize,BOOL ReturnSingleEntry,BOOL RestartScan,unsigned long* pContext,unsigned long* pReturnLength);
	int __stdcall ZwOpenSymbolicLinkObject(HANDLE* LinkHandle,int DesiredAccess,_OBJECT_ATTRIBUTES* ObjectAttributes);
	int __stdcall ZwQuerySymbolicLinkObject(HANDLE LinkHandle,_UNICODE_STRING* LinkTarget,unsigned long* ReturnedLength);

	int __stdcall ZwQueryObject(HANDLE,unsigned long Class,void* Buffer,unsigned long BufferSize,unsigned long* pReturnedLength);
	int __stdcall ZwClose(HANDLE);
}

wchar_t* Print_Level(int lvl)
{
	if(!lvl) return L"";
	else if(lvl==1) return L"=>";
	else if(lvl==2) return L"==>";

	wchar_t X = '=';

	wchar_t* p = (wchar_t*)LocalAlloc(LMEM_ZEROINIT,(lvl*2)+2+2); //Never freed, fix later
	
	int lvl_x = lvl, i =0;

	while(lvl_x--) p[i++]=X;
	p[i]='>';
	return p;
}

BOOL ShouldConcat(wchar_t* pStr)
{
	if(!pStr) return FALSE;
	unsigned long Len = wcslen(pStr);
	if(Len)
	{
		if(pStr[Len-1]=='\\') return FALSE;
	}
	
	return TRUE;
}

int Recur(HANDLE hDirectory,wchar_t* DirName,int LevelX)
{
	unsigned long BufSize = 0;
	void* Buffer = 0;

	unsigned long Context=0;
	int ret =0;

	while( ret >= 0 )
	{

			unsigned long ReturnLength = 0;
			ret = ZwQueryDirectoryObject(hDirectory,0,0,TRUE,FALSE,&Context,&ReturnLength);
			wprintf(L"%s ret: %X, Context: %X, ReturnLength: %X\r\n",Print_Level(LevelX),ret,Context,ReturnLength);

			
			
			
			if(ret == STATUS_BUFFER_TOO_SMALL)
			{
				BufSize = ReturnLength;
				Buffer = LocalAlloc(LMEM_ZEROINIT,BufSize);
				ret = ZwQueryDirectoryObject(hDirectory,Buffer,BufSize,TRUE,FALSE,&Context,&ReturnLength);

				wprintf(L"%s ret: %X, Context: %X, ReturnLength: %X\r\n",Print_Level(LevelX),ret,Context,ReturnLength);

				_UNICODE_STRING* pUni = (_UNICODE_STRING*)Buffer;

				char* StartBuffer = (char*)Buffer;
				char* EndBuffer = ((char*)Buffer)+ReturnLength;

				char* Cur = (char*) pUni;

				char* Str1 = (char*)(pUni->Buffer);
				unsigned long Len1 = pUni->MaxLength;


				char* Str2 =(char*)( (pUni+1)->Buffer);
				unsigned long Len2 = (pUni+1)->MaxLength;

				if( (Cur >= StartBuffer) &&  (Cur+(sizeof(_UNICODE_STRING)*2) <= EndBuffer) /*At least two _UNICODE_STRING structures*/
					&&
					(Str1 >= StartBuffer) &&  (Str1+Len1 <= EndBuffer) 
					&&
					(Str2 >= StartBuffer) &&  (Str2+Len2 <= EndBuffer) )
				{
						wchar_t* Name = 0, *Type =0;
						if(pUni->Length != 0 && pUni->MaxLength != 0 && pUni->Buffer != 0) Name = pUni->Buffer;
						pUni++;
						if(pUni->Length != 0 && pUni->MaxLength != 0 && pUni->Buffer != 0) Type = pUni->Buffer;
						wchar_t FullObjName[MAX_PATH+1]={0};
						if(Name && Type)
						{
							wprintf(L"%s Name: %s, Type: %s\r\n",Print_Level(LevelX),Name,Type);
							
							wcscpy(FullObjName,DirName);
							if(ShouldConcat(FullObjName))	wcscat(FullObjName,L"\\");
							wcscat(FullObjName,Name);
							wprintf(L"%s Full Object Name: %s\r\n",Print_Level(LevelX),FullObjName);
						}
						else wprintf(L"%s Error, Name: %X, Type: %X\r\n",Print_Level(LevelX),Name,Type);

						if( !_wcsicmp(Type,L"Directory") )
						{
							_UNICODE_STRING UNI_S = {0};

							wchar_t* Dir_S = Name;

							UNI_S.Length=wcslen(Dir_S)*2;
							UNI_S.MaxLength=UNI_S.Length+2;
							UNI_S.Buffer=Dir_S;

							_OBJECT_ATTRIBUTES ObjAttr_S = {sizeof(ObjAttr_S)};
							ObjAttr_S.RootDirectory = hDirectory;
							ObjAttr_S.ObjectName=&UNI_S;
							ObjAttr_S.Attributes=0x40;

							HANDLE hDir_S = 0;
							int ret_S = ZwOpenDirectoryObject(&hDir_S,0x20001,&ObjAttr_S);
							wprintf(L"%s ZwOpenDirectoryObject, ret: %X, HANDLE: %X\r\n",Print_Level(LevelX),ret_S,hDir_S);
							if(ret_S < 0)
							{
								wprintf(L"%s Error enumerating subdirectory\r\n",Print_Level(LevelX));
							}
							else
							{
								if(ShouldConcat(FullObjName)) wcscat(FullObjName,L"\\");
								int R = Recur(hDir_S,FullObjName,LevelX+1);
								ZwClose(hDir_S);
							}
						}
						else if( !_wcsicmp(Type,L"SymbolicLink") )
						{
							HANDLE hSymLink = 0;

							_UNICODE_STRING UNI_SL = {0};
							
							UNI_SL.Length=wcslen(FullObjName)*2;
							UNI_SL.MaxLength= UNI_SL.Length + 2;
							UNI_SL.Buffer=FullObjName;

							_OBJECT_ATTRIBUTES ObjAttr_SL = {sizeof(ObjAttr_SL)};
							ObjAttr_SL.ObjectName=&UNI_SL;
							ObjAttr_SL.Attributes=0x40;
							
							int ret_SL = ZwOpenSymbolicLinkObject(&hSymLink,0x80020001,&ObjAttr_SL);
							wprintf(L"%s ZwOpenSymbolicLinkObject, ret: %X, Handle: %X\r\n",Print_Level(LevelX),ret_SL,hSymLink);
							if(ret_SL < 0)
							{
								wprintf(L"%s Error Opening symbolic link \"%s\"\r\n",Print_Level(LevelX),FullObjName);
							}
							else
							{
								unsigned long RetLength = 0;
								
								wchar_t SymTarget[MAX_PATH+2]={0};
								_UNICODE_STRING UNI_SLX={0};
								UNI_SLX.Length= MAX_PATH*2;
								UNI_SLX.MaxLength=UNI_SLX.Length + 2;
								UNI_SLX.Buffer=SymTarget;


								int ret_SLX = ZwQuerySymbolicLinkObject(hSymLink,&UNI_SLX,&RetLength);
								wprintf(L"%s ZwQuerySymbolicLinkObject, ret: %X, Returned Length: %X\r\n",Print_Level(LevelX),ret_SLX,RetLength);
								if(ret_SLX < 0)
								{
									wprintf(L"%s Error ZwQuerySymbolicLinkObject, ret: %X\r\n",Print_Level(LevelX),ret_SLX);
								}
								else
								{
									wprintf(L"%s Link Target: %s\r\n",Print_Level(LevelX),SymTarget);
								}
								ZwClose(hSymLink);
							}
							
						}
				}
				else
				{
					wprintf(L"%s Boundary error\r\n",Print_Level(LevelX));
				}
				wprintf(L"%s ====================\r\n",Print_Level(LevelX));
				if(ret == STATUS_NO_MORE_ENTRIES)
				{
					wprintf(L"%s Enumeration Done------------\r\n",Print_Level(LevelX));
					break;
				}
			}
	}


	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{

	_UNICODE_STRING UNI = {0};
	wchar_t* Dir = L"\\";
	UNI.Length=wcslen(Dir)*2;
	UNI.MaxLength=UNI.Length+2;
	UNI.Buffer=Dir;

	_OBJECT_ATTRIBUTES ObjAttr = {sizeof(ObjAttr)};
	ObjAttr.ObjectName=&UNI;
	ObjAttr.Attributes=0x40;

	HANDLE hDir = 0;
	int ret = ZwOpenDirectoryObject(&hDir,0x20001,&ObjAttr);
	wprintf(L"ZwOpenDirectoryObject, ret: %X, HANDLE: %X\r\n",ret,hDir);


	

	if(ret >= 0)
	{
		Recur(hDir,L"\\",1);
		ZwClose(hDir);
	}

	return 0;
}

