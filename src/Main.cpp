#include "Main.h"
#include "Gui/TrackingWindow.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstring>

using namespace cv;
using namespace std;

Scalar myColors[] = {
	{197, 100, 195},
	{90, 56, 200},
	{67, 163, 255},
	{143, 176, 59},
	{206, 29, 255},
	{133, 108, 252},
	{166, 252, 153},
	{120, 128, 21},
	{93, 236, 178},
	{196, 108, 43},
	{74, 110, 255},
	{20, 249, 29},
	{208, 173, 162}
};

Scalar PickColor(int index)
{
	if (index > sizeof(myColors))
		index = index % sizeof(myColors);

	return myColors[index];
}

string TargetTypeToString(TARGET_TYPE t)
{
	switch (t) {
	case TYPE_MALE:
		return "Male";
	case TYPE_FEMALE:
		return "Female";
	case TYPE_BACKGROUND:
		return "Background";
	default:
		return "Unknown";
	}
}

string TrackingModeToString(TrackingMode m)
{
	switch (m) {
	case TM_DIAGONAL:
		return "Diagonal";
	case TM_HORIZONTAL:
		return "Horizontal";
	case TM_VERTICAL:
		return "Vertical";
	case TM_DIAGONAL_SINGLE:
		return "X/Y move";
	case TM_HORIZONTAL_SINGLE:
		return "X move";
	case TM_VERTICAL_SINGLE:
		return "Y move";
	default:
		return "None";
	}
}

int main(int argc, char* argv[])
{
	cout << "Starting" << endl;


	//const string fName = "H:\\P\\Vr\\Script\\Videos\\Mine\\DIRTY TALKING SLUT RAM PLAYS WITH SEX MACHINE FANTASIZING ABOUT SUBARU_Yukki Amey_1080p.mp4";
	//const String fName = "C:\\dev\\opencvtest\\build\\Latex_Nurses_Scene_2.mp4";
	//string fName = "H:\\P\\Vr\\Script\\Videos\\Fuck\\Best Throat Bulge Deepthroat Ever. I gave my Hubby ASIAN ESCORT as a gift.mp4";
	string fName = "H:\\P\\Vr\\Script\\Videos\\Mine\\Zero Two sucking dick and getting hard fuck_Fucking Lina.mp4";
	//string fName = "H:\\P\\Vr\\Script\\Videos\\Mine\\H9BQSO~J.MP4";
	//string fName = "H:\\P\\Vr\\Script\\Videos\\Mine\\FRUWJ9~3.MP4";
	if (argc > 1)
		fName = argv[1];
	
	TrackingWindow win(
		fName
	);
	win.Run();

	return 0;
}