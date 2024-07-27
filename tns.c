//
// tns.c v2.1
// Copyright © 2001, 2015, Goran Kataviæ <gkatavic@protonmail.com>
// GNU General Public License v3.0
//
#include <windows.h>
#include <winreg.h>
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <alloc.h>
#include <dirent.h>
#include <direct.h>
//
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
// datetime
#include <time.h>
#include <sys/stat.h>
#include <utime.h>

#define KEY_WOW64_64KEY	0x0100
#define KEY_WOW64_32KEY	0x0200

#define ENABLE_ECHO_INPUT 0x0004
#define ENABLE_EXTENDED_FLAGS 0x0080
#define ENABLE_INSERT_MODE 0x0020 
#define ENABLE_LINE_INPUT 0x0002
#define ENABLE_MOUSE_INPUT 0x0010
#define ENABLE_PROCESSED_INPUT 0x0001
#define ENABLE_QUICK_EDIT_MODE 0x0040 
#define ENABLE_WINDOW_INPUT 0x0008 

#if !defined(MAXPATHLEN)
#define MAXPATHLEN  512
#endif

#define REGISTRY_PATH_CONFIG "SOFTWARE\\ORACLE"
#define COPY_OK "OK"
#define COPY_ERROR "ERROR!"


char exedir[512];
char applicationpath[512];
//
char main_oracle_home_64[512];
char main_oracle_home_32[512];
char oracle_home_64[512];
char oracle_home_32[512];
//
char tnsnames_ora[512];
char sqlnet_ora[512];


typedef int bool;
enum { false, true };

typedef struct _CONSOLE_FONT_INFOEX {
  ULONG cbSize;
  DWORD nFont;
  COORD dwFontSize;
  UINT  FontFamily;
  UINT  FontWeight;
  WCHAR FaceName[LF_FACESIZE];
} CONSOLE_FONT_INFOEX, *PCONSOLE_FONT_INFOEX;
 
typedef BOOL (WINAPI *SETCONSOLEFONT)(HANDLE, DWORD);
SETCONSOLEFONT SetConsoleFont;

typedef BOOL (WINAPI *SETCURRENTCONSOLEFONTEX)(HANDLE,BOOL, PCONSOLE_FONT_INFOEX);
SETCURRENTCONSOLEFONTEX SetCurrentConsoleFontEx;

typedef struct _ORACLE_HOME {
  char oracle_home_path[512];
  char oracle_home_regkey[512];
  unsigned int xbit;
} ORACLE_HOME;


BOOL Is64BitOS(void)
{
	BOOL bIs64Bit = FALSE;
 
	#if defined(_WIN64)
 
	bIs64Bit = TRUE;  // 64-bit programs run only on Win64

	#elif defined(_WIN32)
 
	// Note that 32-bit programs run on both 32-bit and 64-bit Windows

	typedef BOOL (WINAPI *LPFNISWOW64PROCESS) (HANDLE, PBOOL);
	LPFNISWOW64PROCESS pfnIsWow64Process = (LPFNISWOW64PROCESS)GetProcAddress(GetModuleHandle(_T("kernel32")), "IsWow64Process");
 
	if (pfnIsWow64Process)
		pfnIsWow64Process(GetCurrentProcess(), &bIs64Bit);
 
	#endif
 
	return bIs64Bit;
}


char* concat(int count, ...)
{
    va_list ap;
    int i;
	char *merged;
	int null_pos;

    // Find required length to store merged string
    int len = 1; // room for NULL
    va_start(ap, count);
    for(i=0 ; i<count ; i++)
        len += strlen(va_arg(ap, char*));
    va_end(ap);

    // Allocate memory to concat strings
    merged = calloc(sizeof(char),len);
    null_pos = 0;

    // Actually concatenate strings
    va_start(ap, count);
    for(i=0 ; i<count ; i++)
    {
        char *s = va_arg(ap, char*);
        strcpy(merged+null_pos, s);
        null_pos += strlen(s);
    }
    va_end(ap);

    return merged;
}


bool file_exists(const char *filename)
{
	FILE *file;
    file = fopen(filename, "r");
	if (file != NULL)
    {
        fclose(file);
        return true;
    }
    return false;
}


bool directory_exists(const char *dirname)
{
	struct stat s;
	int err = stat(dirname, &s);
	if(err == -1) 
	{
		if(ENOENT == errno) 
		{
			/* does not exist */
			return false;
		}
		else 
		{
			// stat error
			return false;
		}
	} 
	else 
	{
		if(S_ISDIR(s.st_mode)) 
		{
			/* it's a dir */
			return true;
		} 
		else
		{
			/* exists but is no dir */
			return false;
		}
	}	
}


char *dirname(CONST char *path)
{
	static char *bname;
	CONST char *endp;

	if (bname == NULL) 
	{
		bname = (char *) malloc(MAXPATHLEN);
		if (bname == NULL)
		{
			return (NULL);
		}
	}

	/* Empty or NULL string gets treated as "." */
	if (path == NULL || *path == '\0') 
	{
		strncpy(bname, ".", MAXPATHLEN);
		return (bname);
	}

	/* Strip trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '\\')
	{
		endp--;
	}

	/* Find the start of the dir */
	while (endp > path && *endp != '\\')
	{
		endp--;
	}

	/* Either the dir is "\\" or there are no slashes */
	if (endp == path) 
	{
		strncpy(bname, *endp == '\\' ? "\\" : ".", MAXPATHLEN);
		return (bname);
	}

	do 
	{
		endp--;
	} while (endp > path && *endp == '\\');

	if (endp - path + 2 > MAXPATHLEN)
	{
		errno = -1;
		return (NULL);
	}

	strcpy(bname, path);
	bname[(endp - path) + 1] = 0;

	return (bname);
}


/* Standard error macro for reporting API errors */ 
 #define PERR(bSuccess, api){if(!(bSuccess)) printf("%s:Error %d from %s on line %d\n", __FILE__, GetLastError(), api, __LINE__);}
 
 void cls( HANDLE hConsole )
 {
    COORD coordScreen = { 0, 0 };    /* here's where we'll home the
                                        cursor */ 
    BOOL bSuccess;
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi; /* to get buffer info */ 
    DWORD dwConSize;                 /* number of character cells in
                                        the current buffer */ 

    /* get the number of character cells in the current buffer */ 
    bSuccess = GetConsoleScreenBufferInfo( hConsole, &csbi );
    PERR( bSuccess, "GetConsoleScreenBufferInfo" );
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

    /* fill the entire screen with blanks */ 
    bSuccess = FillConsoleOutputCharacter( hConsole, (TCHAR) ' ',
       dwConSize, coordScreen, &cCharsWritten );
    PERR( bSuccess, "FillConsoleOutputCharacter" );

    /* get the current text attribute */ 
    bSuccess = GetConsoleScreenBufferInfo( hConsole, &csbi );
    PERR( bSuccess, "ConsoleScreenBufferInfo" );

    /* now set the buffer's attributes accordingly */ 
    bSuccess = FillConsoleOutputAttribute( hConsole, csbi.wAttributes,
       dwConSize, coordScreen, &cCharsWritten );
    PERR( bSuccess, "FillConsoleOutputAttribute" );

    /* put the cursor at (0, 0) */ 
    bSuccess = SetConsoleCursorPosition( hConsole, coordScreen );
    PERR( bSuccess, "SetConsoleCursorPosition" );
	
    return;
 }
 
 
 void SetConsoleWindowSize(int Width, int Height, int BufferWidth, int BufferHeight)
{
    COORD coord;
	SMALL_RECT Rect;
	HANDLE Handle = GetStdHandle(STD_OUTPUT_HANDLE);      // Get Handle
	
    coord.X = BufferWidth;
    coord.Y = BufferHeight;
    
    Rect.Top = 0;
    Rect.Left = 0;
    Rect.Bottom = Height - 1;
    Rect.Right = Width - 1;

    SetConsoleScreenBufferSize(Handle, coord);            // Set Buffer Size
    SetConsoleWindowInfo(Handle, TRUE, &Rect);            // Set Window Size
}

 
void addorahome(struct _ORACLE_HOME *orahomesreference, char *oracle_home, char *oracle_regkey, unsigned int xb, unsigned int *ohcount)
{   
	unsigned int hcnt;
	BOOL bIsDuplicateHomePath = FALSE;
	
	if (*ohcount > 0)
	{
		for (hcnt=0; hcnt<*ohcount; hcnt++)
		{
			if (!strcmp((char *)oracle_home, (char *)&orahomesreference[hcnt].oracle_home_path))
			{
				strcpy((char *)&orahomesreference[hcnt].oracle_home_regkey, (char *)oracle_regkey);
				orahomesreference[hcnt].xbit = xb;
				//printf("\n%d - %s - %s\n", *ohcount, oracle_home, orahomesreference[hcnt].oracle_home_path);
				bIsDuplicateHomePath = TRUE;
			}
		}
	}
	
	if (!bIsDuplicateHomePath)
	{
		orahomesreference = realloc(orahomesreference, (*ohcount + 1) * sizeof(struct _ORACLE_HOME));
		if (orahomesreference != NULL)
		{
			strcpy((char *)&orahomesreference[*ohcount].oracle_home_path, (char *)oracle_home);
			strcpy((char *)&orahomesreference[*ohcount].oracle_home_regkey, (char *)oracle_regkey);
			orahomesreference[*ohcount].xbit = xb;
			//printf("\n%d - %s ------- %s\n", *ohcount, oracle_home, orahomesreference[*ohcount].oracle_home_path);
			*ohcount = *ohcount + 1;	
		}
		else
		{
			free (orahomesreference);
			puts ("Error (re)allocating memory");
			getch();
			exit (1);
		}
	}
 }
 


void consolepause (void)
{
	printf("\r\nPress any key to continue . . .");
	getch();
	printf("\r\n");
}


BOOL append_file (char *src, char *dst)
{
  FILE *ifp, *ofp;
  char buf[1024];

  if ((ifp = fopen(src, "rb")) == NULL)
  {
    //perror("open source-file");
    return FALSE;
  }
  
  if ((ofp = fopen(dst, "ab")) == NULL)
  {
    //perror("open target-file");
    return FALSE;
  }
  
  while (fgets(buf, sizeof(buf), ifp) != NULL)
  { /* While we don't reach the end of source. */
    /* Read characters from source file to fill buffer. */
    /* Write characters read to target file. */
    fwrite(buf, sizeof(char), strlen(buf), ofp);
  }
  
  fclose(ifp);
  fclose(ofp);
  
  return TRUE;
}


BOOL copy_file (char *src, char *dst)
{
  FILE *ifp, *ofp;
  char buf[1024];
  struct stat src_stat_buf;
  struct utimbuf src_dt;

  if ((ifp = fopen(src, "rb")) == NULL)
  {
    //perror("open source-file");
    return FALSE;
  }
  
  if ((ofp = fopen(dst, "wb")) == NULL)
  {
    //perror("open target-file");
    return FALSE;
  }
  
  while (fgets(buf, sizeof(buf), ifp) != NULL)
  { /* While we don't reach the end of source. */
    /* Read characters from source file to fill buffer. */
    /* Write characters read to target file. */
    fwrite(buf, sizeof(char), strlen(buf), ofp);
  }
  
  fclose(ifp);
  fclose(ofp);
  
  if (!stat(src, &src_stat_buf))
  {
		//src_dt.actime = time(NULL); /* set atime to current time */
		src_dt.actime = src_stat_buf.st_atime; /* keep atime unchanged */		
		src_dt.modtime = src_stat_buf.st_mtime; /* keep mtime unchanged */
		utime(dst, &src_dt);		
  }
  
  return TRUE;
}


void CopyToNetAdminDir(char *netadmindir)
{
	BOOL copyflag;
	char *orahome_tnsnames_ora = concat (2, netadmindir, "\\tnsnames.ora");
	char *orahome_sqlnet_ora = concat (2, netadmindir, "\\sqlnet.ora");
	char *orahome_tnsnames_config_ora = concat (2, netadmindir, "\\tnsnames-config.ora");
	char *orahome_sqlnet_config_ora = concat (2, netadmindir, "\\sqlnet-config.ora");

	// tnsnames.ora
	if (file_exists(tnsnames_ora))
	{
		copyflag = copy_file(tnsnames_ora, orahome_tnsnames_ora);
		printf("  tnsnames.ora -> %s - %s\r\n", orahome_tnsnames_ora, copyflag ? COPY_OK : COPY_ERROR);
	}
	if (file_exists(orahome_tnsnames_config_ora))
	{
		copyflag = append_file(orahome_tnsnames_config_ora, orahome_tnsnames_ora);
		printf("    + {ORACLE_HOME}\\NETWORK\\ADMIN\\tnsnames-config.ora >> %s - %s\r\n", orahome_tnsnames_ora, copyflag ? COPY_OK : COPY_ERROR);
	}
	
	// sqlnet.ora
	if (file_exists(orahome_sqlnet_config_ora))
	{
		copyflag = copy_file(orahome_sqlnet_config_ora, orahome_sqlnet_ora);
		printf("  {ORACLE_HOME}\\NETWORK\\ADMIN\\sqlnet-config.ora -> %s - %s\r\n", orahome_sqlnet_ora, copyflag ? COPY_OK : COPY_ERROR);
	}
	else if (file_exists(sqlnet_ora))
	{
		copyflag = copy_file(sqlnet_ora, orahome_sqlnet_ora);
		printf("  sqlnet.ora -> %s - %s\r\n", orahome_sqlnet_ora, copyflag ? COPY_OK : COPY_ERROR);
	}
}


int main (int argc, char *argv[])
{
	LONG result;
	HKEY skey, rootskey;
	DWORD valtype, valsize, valsize_subkeyname;
	char subkeyname[512];
	char REGISTRY_PATH_CONFIG_X[512];
	unsigned long x, y;
	DIR *dirp;
	//
	FILETIME ft;
	//
	ORACLE_HOME *orahomes;
	unsigned int orahomes_count = 0;
	char *orahome_network_dir;
	char *orahome_net80_dir;
	//
	HANDLE hConsoleHandle;
	HANDLE hConsoleInputHandle;
	//
	WORD wOldColorAttrs;
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
	//
	OSVERSIONINFOEX osvi;
	CONSOLE_FONT_INFOEX cfi;
	//
	HMODULE hmod = GetModuleHandle(_T("KERNEL32.DLL"));
	
	strcpy(exedir, dirname(argv[0]));
	strcpy(applicationpath, argv[0]);
	
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx((OSVERSIONINFO *) &osvi) ;

	if (argc > 0)
	{
		// Avoid compile warning!
	}
	
	if (osvi.dwMajorVersion < 6) // XP
	{
		SetConsoleFont = (SETCONSOLEFONT) GetProcAddress(hmod, "SetConsoleFont");
		SetConsoleFont(GetStdHandle(STD_OUTPUT_HANDLE), 6);	 // Lucida Console 12 on XP (undocumented feature !!!)
	}
	else // Vista, Win7, Win8 ...
	{
		SetCurrentConsoleFontEx =(SETCURRENTCONSOLEFONTEX) GetProcAddress(hmod, "SetCurrentConsoleFontEx");
		//
		ZeroMemory(&cfi, sizeof(CONSOLE_FONT_INFOEX));
		cfi.cbSize=sizeof(CONSOLE_FONT_INFOEX);
		cfi.cbSize = sizeof cfi;
		cfi.nFont = 0;
		cfi.dwFontSize.X = 0;
		cfi.dwFontSize.Y = 12;
		cfi.FontFamily = FF_DONTCARE;
		cfi.FontWeight = FW_NORMAL;
		wcscpy(cfi.FaceName, L"Lucida Console");
		SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), false, &cfi);
	}
	
	// Handle to write to the console
	hConsoleHandle = GetStdHandle ( STD_OUTPUT_HANDLE );
			
	// Handle to input to the console
	hConsoleInputHandle = GetStdHandle ( STD_INPUT_HANDLE );
    
	// Create a COORD to hold the buffer size:  
	// SetConsoleScreenBufferSize(hConsoleHandle, bufferSize);
			
	// Change the console window size:
	// SetConsoleWindowInfo(hConsoleHandle, 1, &windowSize);
			
	SetConsoleWindowSize (132, 48, 512, 256);

	// Set code page to 1250	
	SetConsoleOutputCP(1250);
	SetConsoleCP(1250);
			
	// Enable Quick Edit Mode			
	SetConsoleMode(hConsoleInputHandle,
			ENABLE_ECHO_INPUT
			| ENABLE_EXTENDED_FLAGS
			| ENABLE_INSERT_MODE
			| ENABLE_LINE_INPUT
			| ENABLE_MOUSE_INPUT
			| ENABLE_PROCESSED_INPUT
			| ENABLE_QUICK_EDIT_MODE
			| ENABLE_WINDOW_INPUT
			);
	  
	/*
	* First save the current color information
	*/
	GetConsoleScreenBufferInfo(hConsoleHandle, &csbiInfo);
	wOldColorAttrs = csbiInfo.wAttributes; 
  
	/*
	* Set the new color information
	*/
	SetConsoleTextAttribute(hConsoleHandle,
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY |
		BACKGROUND_RED);
	
	cls(hConsoleHandle);
	
	SetConsoleTitle("tnsnames.ora & sqlnet.ora copy tool");

	printf ("\r\nTNS v2.1\r\n\r\n");
	printf ("Copyright © 2001, 2015, Goran Kataviæ <gkatavic@protonmail.com> - GPLv3\r\n\r\n");
	puts("  #####################################################################################");
	puts("  # This tool copies files \"tnsnames.ora\" & \"sqlnet.ora\" to all ORACLE HOMES          #");
	puts("  # Exceptions (for files found in {ORACLE_HOME}\\NETWORK\\ADMIN):                      #");
	puts("  #  1) if file \"tnsnames-config.ora\" is found, it will be appended to \"tnsnames.ora\" #");
	puts("  #  2) if file \"sqlnet-config.ora\" is found, it will be copied to \"sqlnet.ora\"       #");
	puts("  #####################################################################################");
		
	orahomes_count = 0;
	orahomes = malloc (sizeof(struct _ORACLE_HOME));
	//
	// 32-bit Oracle Homes
	result = RegOpenKeyEx (HKEY_LOCAL_MACHINE, REGISTRY_PATH_CONFIG, 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &rootskey);
	if (result == ERROR_SUCCESS) 
	{
		valsize = sizeof (main_oracle_home_32);
		result = RegQueryValueEx (rootskey, "ORACLE_HOME", 0, &valtype, (LPBYTE) &main_oracle_home_32, &valsize);
		if (result == ERROR_SUCCESS) 
		{
			//printf ("32-bit: %s - %s\r\n", REGISTRY_PATH_CONFIG, main_oracle_home_32);
			addorahome(orahomes, main_oracle_home_32, REGISTRY_PATH_CONFIG, (unsigned int)32, &orahomes_count);			
		}
		// Idemo po subkeysima
		RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGISTRY_PATH_CONFIG, 0, KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_32KEY, &rootskey);
		valsize_subkeyname = sizeof (subkeyname);
		x = 0;
		while (RegEnumKeyEx(rootskey, x, (LPBYTE) &subkeyname, &valsize_subkeyname, 0, 0, 0, &ft) != ERROR_NO_MORE_ITEMS)
		{
			valsize_subkeyname = sizeof (subkeyname);
			sprintf (REGISTRY_PATH_CONFIG_X, "%s\\%s", REGISTRY_PATH_CONFIG, subkeyname, x);
			result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGISTRY_PATH_CONFIG_X, 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &skey);
			if (result == ERROR_SUCCESS)
			{
				valsize = sizeof (oracle_home_32);
				result = RegQueryValueEx(skey, "ORACLE_HOME", 0, &valtype, (LPBYTE) &oracle_home_32, &valsize);
				if (result == ERROR_SUCCESS)
				{
					//printf ("32-bit: %s - %s\r\n", REGISTRY_PATH_CONFIG_X, oracle_home_32);
					addorahome(orahomes, oracle_home_32, REGISTRY_PATH_CONFIG_X, (unsigned int)32, &orahomes_count);
					//
					RegCloseKey (skey);
				}
			}
			//
			x++;
		} // of while
		//
		RegCloseKey (rootskey);
	}
	//
	if (Is64BitOS())
	{
		//
		// 64-bit Oracle Homes
		result = RegOpenKeyEx (HKEY_LOCAL_MACHINE, REGISTRY_PATH_CONFIG, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &rootskey);
		if (result == ERROR_SUCCESS) 
		{
			valsize = sizeof (main_oracle_home_64);
			result = RegQueryValueEx (rootskey, "ORACLE_HOME", 0, &valtype, (LPBYTE) &main_oracle_home_64, &valsize);
			if (result == ERROR_SUCCESS) 
			{
				//printf ("64-bit: %s - %s\r\n", REGISTRY_PATH_CONFIG, main_oracle_home_64);
				addorahome(orahomes, main_oracle_home_64, REGISTRY_PATH_CONFIG, (unsigned int)64, &orahomes_count);			
			}
			// Idemo po subkeysima
			RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGISTRY_PATH_CONFIG, 0, KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_64KEY, &rootskey);
			valsize_subkeyname = sizeof (subkeyname);
			x = 0;
			while (RegEnumKeyEx(rootskey, x, (LPBYTE) &subkeyname, &valsize_subkeyname, 0, 0, 0, &ft) != ERROR_NO_MORE_ITEMS)
			{
				valsize_subkeyname = sizeof (subkeyname);
				sprintf (REGISTRY_PATH_CONFIG_X, "%s\\%s", REGISTRY_PATH_CONFIG, subkeyname, x);
				result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGISTRY_PATH_CONFIG_X, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &skey);
				if (result == ERROR_SUCCESS)
				{
					valsize = sizeof (oracle_home_64);
					result = RegQueryValueEx(skey, "ORACLE_HOME", 0, &valtype, (LPBYTE) &oracle_home_64, &valsize);
					if (result == ERROR_SUCCESS)
					{
						//printf ("64-bit: %s - %s\r\n", REGISTRY_PATH_CONFIG_X, oracle_home_64);
						addorahome(orahomes, oracle_home_64, REGISTRY_PATH_CONFIG_X, (unsigned int)64, &orahomes_count);
						//
						RegCloseKey (skey);
					}
				}
				//
				x++;
			} // of while
			//
			RegCloseKey (rootskey);		
		}
	} // Is64BitOS
			
			
	printf ("\r\nORACLE HOMES found: [%d]\r\n--\r\n", orahomes_count);
	
	strcpy(tnsnames_ora, concat(3, exedir, "\\", "tnsnames.ora"));
	strcpy(sqlnet_ora, concat(3, exedir, "\\", "sqlnet.ora"));

	if (!file_exists(tnsnames_ora) && !file_exists(sqlnet_ora))
	{
		printf ("\r\n\r\n\ERROR: There are no files 'tnsnames.ora' or 'sqlnet.ora' in folder '%s'!\r\n\r\n", exedir);
	}
	else
	{
		if (orahomes_count > 0)
		{
		
			for (x=0; x < orahomes_count; x++)
			{
				if (x > 0)
				{
					printf ("\r\n");
				}
				printf ("\r\nORACLE HOME %d-bit key: [HKLM\\%s]\r\n", orahomes[x].xbit, orahomes[x].oracle_home_regkey);
				for (y=0; y<(strlen(orahomes[x].oracle_home_regkey)+31); y++)
					printf("=");
				printf("\r\n");
				printf ("  PATH: %s\r\n  --\r\n", orahomes[x].oracle_home_path);
				orahome_network_dir = concat(2, orahomes[x].oracle_home_path, "\\NETWORK\\ADMIN");
				orahome_net80_dir = concat(2, orahomes[x].oracle_home_path, "\\NET80\\ADMIN");
				if (directory_exists(orahome_network_dir))
				{
					CopyToNetAdminDir(orahome_network_dir);
				}
				if (directory_exists(orahome_net80_dir))
				{
					printf ("  --\r\n");
					CopyToNetAdminDir(orahome_net80_dir);
				}
			}

		} // if (orahomes_count > 0)
	} // tnsnames.ora or sqlnet.ora found
	
	printf("\r\n\r\nPress any key to exit . . . ");
	getch();
	
	// Restore the original colors
	SetConsoleTitle("Command Prompt");
	SetConsoleTextAttribute (hConsoleHandle, wOldColorAttrs);
	cls(hConsoleHandle);
  
  return 0;
  
}
