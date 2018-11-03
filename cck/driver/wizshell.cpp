#include "stdafx.h"
#include "globals.h"
#include "WizardMachine.h"
#include "fstream.h"
#include "direct.h"
extern char iniFilePath[MAX_SIZE];
extern char customizationPath[MAX_SIZE];

extern CWizardMachineApp theApp;
extern char currDirPath[MAX_SIZE];
 
//CString CDout="CD_output";

void CreateRshell (void)
{
	CString root   = GetGlobal("Root");
	CString config = GetGlobal("CustomizationList");

	CString destPath = root + "\\Configs\\" + config + "\\Rshell";

	CWnd Mywnd;

//		Mywnd.MessageBox(CString(iniFilePath),iniFilePath,MB_OK);

//		Mywnd.MessageBox(CString(customizationPath),customizationPath,MB_OK);

	ifstream part1("part1.ini");
	ifstream part2("part2.ini");
	_mkdir (destPath);
	CString Rsh = destPath + "\\" +"rshell.ini";
//	FILE* rshell = theApp.OpenAFile(CDdir +"rshell.ini", "w");

	ofstream rshell(Rsh);
	CString fvalue1=GetGlobal("ShellTitleText");
	CString fvalue2=GetGlobal("ShellBgBitmap");
//	char *fvalue3=GetGlobal("dialog_title_text");
	CString fvalue4=GetGlobal("ShellInstallTextFile");
	char jsprefname[200];

	if(!part1) {
		cout << "cannot open the file \n";
		}
	while (!part1.eof()) {
	
		part1.getline(jsprefname,200);
//		fprintf(globs, jsprefname);
//		fprintf(globs, "\n");

		rshell <<jsprefname<<"\n";
	}
	rshell <<"caption="<<fvalue1<<"\n";
	rshell <<"bk_bitmap="<<fvalue2<<"\n";
	rshell <<"button2_cmdline="<<fvalue4<<"\n";

//	rshell <<"dialog_title_txt="<<fvalue3<<"\n";
	if(!part2) {
		cout << "cannot open the file \n";
		}
	while (!part2.eof()) {
	
		part2.getline(jsprefname,200);
		rshell <<jsprefname<<"\n";
	}
	rshell.close();

}
