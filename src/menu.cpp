/***************************************************************************
 *   Copyright (C) 2006 by Massimiliano Torromeo   *
 *   massimiliano.torromeo@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <algorithm>
#include <math.h>
#include <unistd.h>
#include "gmenu2x.h"
#include "linkapp.h"
#include "menu.h"
#include "filelister.h"
#include "utilities.h"
#include "pxml.h"

using namespace std;

Menu::Menu(GMenu2X *gmenu2x) {
	this->gmenu2x = gmenu2x;
	iFirstDispSection = 0;

	DIR *dirp;
	struct stat st;
	struct dirent *dptr;
	string filepath;

	if ((dirp = opendir("sections")) == NULL) return;

	while ((dptr = readdir(dirp))) {
		if (dptr->d_name[0]=='.') continue;
		filepath = (string)"sections/"+dptr->d_name;
		int statRet = stat(filepath.c_str(), &st);
		if (!S_ISDIR(st.st_mode)) continue;
		if (statRet != -1) {
			sections.push_back((string)dptr->d_name);
			linklist ll;
			links.push_back(ll);
		}
	}

	closedir(dirp);
	sort(sections.begin(),sections.end(),case_less());
	setSectionIndex(0);
	readLinks();
}

Menu::~Menu() {
	freeLinks();
}

uint Menu::firstDispRow() {
	return iFirstDispRow;
}

void Menu::loadIcons() {
	//reload section icons
	for (uint i=0; i<sections.size(); i++) {
		string sectionIcon = "sections/"+sections[i]+".png";
		if (!gmenu2x->sc.getSkinFilePath(sectionIcon).empty())
			gmenu2x->sc.add("skin:"+sectionIcon);

		//check link's icons
		string linkIcon;
		for (uint x=0; x<sectionLinks(i)->size(); x++) {
			linkIcon = sectionLinks(i)->at(x)->getIcon();
			LinkApp *linkapp = dynamic_cast<LinkApp*>(sectionLinks(i)->at(x));

			if (linkIcon.substr(0,5)=="skin:") {
				linkIcon = gmenu2x->sc.getSkinFilePath(linkIcon.substr(5,linkIcon.length()));
				if (linkapp != NULL && !fileExists(linkIcon))
					linkapp->searchIcon();
				else
					sectionLinks(i)->at(x)->setIconPath(linkIcon);

			} else if (!fileExists(linkIcon)) {
				if (linkapp != NULL) linkapp->searchIcon();
			}
		}
	}
}

/*====================================
   SECTION MANAGEMENT
  ====================================*/
void Menu::freeLinks() {
	for (vector<linklist>::iterator section = links.begin(); section<links.end(); section++)
		for (linklist::iterator link = section->begin(); link<section->end(); link++)
			free(*link);
}

linklist *Menu::sectionLinks(int i) {
	if (i<0 || i>(int)links.size())
		i = selSectionIndex();

	if (i<0 || i>(int)links.size())
		return NULL;

	return &links[i];
}

void Menu::decSectionIndex() {
	setSectionIndex(iSection-1);
}

void Menu::incSectionIndex() {
	setSectionIndex(iSection+1);
}

uint Menu::firstDispSection() {
	return iFirstDispSection;
}

int Menu::selSectionIndex() {
	return iSection;
}

string Menu::selSection() {
	return sections[iSection];
}

void Menu::setSectionIndex(int i) {
	if (i<0)
		i=sections.size()-1;
	else if (i>=(int)sections.size())
		i=0;
	iSection = i;

	if (i>(int)iFirstDispSection+4)
		iFirstDispSection = i-4;
	else if (i<(int)iFirstDispSection)
		iFirstDispSection = i;

	iLink = 0;
	iFirstDispRow = 0;
}

string Menu::sectionPath(int section) {
	if (section<0 || section>(int)sections.size()) section = iSection;
	return "sections/"+sections[section]+"/";
}

/*====================================
   LINKS MANAGEMENT
  ====================================*/
bool Menu::addActionLink(uint section, string title, LinkRunAction action, string description, string icon) {
	if (section>=sections.size()) return false;

	LinkAction *linkact = new LinkAction(gmenu2x,action);
	linkact->setSize(gmenu2x->skinConfInt["linkWidth"],gmenu2x->skinConfInt["linkHeight"]);
	linkact->setTitle(title);
	linkact->setDescription(description);
	if (gmenu2x->sc.exists(icon) || (icon.substr(0,5)=="skin:" && !gmenu2x->sc.getSkinFilePath(icon.substr(5,icon.length())).empty()) || fileExists(icon))
	linkact->setIcon(icon);

	sectionLinks(section)->push_back(linkact);
	return true;
}

bool Menu::addLink(string path, string file, string section) {
	if (section=="")
		section = selSection();
	else if (find(sections.begin(),sections.end(),section)==sections.end()) {
		//section directory doesn't exists
		if (!addSection(section))
			return false;
	}
	if (path[path.length()-1]!='/') path += "/";

	//if the extension is not equal to gpu or dge then enable the wrapepr by default
	bool wrapper = false, pxml = false;

	//strip the extension from the filename
	string title = file;
	string::size_type pos = title.rfind(".");
	if (pos!=string::npos && pos>0) {
		string ext = title.substr(pos, title.length());
		transform(ext.begin(), ext.end(), ext.begin(), (int(*)(int)) tolower);
		if (ext == ".gpu" || ext == ".dge") wrapper = false;
		else if (ext == ".pxml") pxml = true;
		title = title.substr(0, pos);
	}

	string linkpath = "sections/"+section+"/"+title;
	int x=2;
	while (fileExists(linkpath)) {
		stringstream ss;
		linkpath = "";
		ss << x;
		ss >> linkpath;
		linkpath = "sections/"+section+"/"+title+linkpath;
		x++;
	}
#ifdef DEBUG
	cout << "\033[0;34mGMENU2X:\033[0m Adding link: " << linkpath << endl;
#endif

	//search for a manual
	pos = file.rfind(".");
	string exename = path+file.substr(0,pos);
	string manual = "";
	if (fileExists(exename+".man.png")) {
		manual = exename+".man.png";
	} else if (fileExists(exename+".man.jpg")) {
		manual = exename+".man.jpg";
	} else if (fileExists(exename+".man.jpeg")) {
		manual = exename+".man.jpeg";
	} else if (fileExists(exename+".man.bmp")) {
		manual = exename+".man.bmp";
	} else if (fileExists(exename+".man.txt")) {
		manual = exename+".man.txt";
	} else {
		//scan directory for a file like *readme*
		FileLister fl(path, false);
		fl.setFilter(".txt");
		fl.browse();
		bool found = false;
		for (uint x=0; x<fl.size() && !found; x++) {
			string lcfilename = fl[x];

			if (lcfilename.find("readme") != string::npos) {
				found = true;
				manual = path+fl.files[x];
			}
		}
	}
#ifdef DEBUG
	cout << "\033[0;34mGMENU2X:\033[0m Manual: " << manual << endl;
#endif

	// Read pxml
	string shorttitle="", description="", exec="", icon="";
	if (pxml) {
		PXml pxmlDoc(path+file);
		if (pxmlDoc.isValid()) {
			shorttitle = pxmlDoc.getTitle();
			description = pxmlDoc.getDescription();
			exec = pxmlDoc.getExec();
			if (!exec.empty() && exec[0]!='/')
				exec = path+exec;
			icon = pxmlDoc.getIcon();
			if (!icon.empty() && icon[0]!='/')
				icon = path+icon;
		} else {
#ifdef DEBUG
			cout << "\033[0;34mGMENU2X:\033[0m Error loading pxml " << file << ": " << pxmlDoc.getError() << endl;
#endif
			return false;
		}
	} else {
		shorttitle = title;
		exec = path+file;
	}

	//Reduce title lenght to fit the link width
	if (gmenu2x->font->getTextWidth(shorttitle)>gmenu2x->skinConfInt["linkWidth"]) {
		while (gmenu2x->font->getTextWidth(shorttitle+"..")>gmenu2x->skinConfInt["linkWidth"])
			shorttitle = shorttitle.substr(0,shorttitle.length()-1);
		shorttitle += "..";
	}

	ofstream f(linkpath.c_str());
	if (f.is_open()) {
		f << "title=" << shorttitle << endl;
		f << "exec=" << exec << endl;
		if (!description.empty()) f << "description=" << description << endl;
		if (!icon.empty()) f << "icon=" << icon << endl;
		if (!manual.empty()) f << "manual=" << manual << endl;
		if (wrapper) f << "wrapper=true" << endl;
		f.close();
 		sync();
		int isection = find(sections.begin(),sections.end(),section) - sections.begin();
		if (isection>=0 && isection<(int)sections.size()) {
#ifdef DEBUG
			cout << "\033[0;34mGMENU2X:\033[0m Section: " << sections[isection] << "(" << isection << ")" << endl;
#endif
			LinkApp* link = new LinkApp(gmenu2x, linkpath.c_str());
			link->setSize(gmenu2x->skinConfInt["linkWidth"],gmenu2x->skinConfInt["linkHeight"]);
			links[isection].push_back( link );
		}
	} else {
#ifdef DEBUG
		cout << "\033[0;34mGMENU2X:\033[0;31m Error while opening the file '" << linkpath << "' for write\033[0m" << endl;
#endif
		return false;
	}

	return true;
}

bool Menu::addSection(string sectionName) {
	string sectiondir = "sections/"+sectionName;
	if (mkdir(sectiondir.c_str(),0777)==0) {
		sections.push_back(sectionName);
		linklist ll;
		links.push_back(ll);
		return true;
	}
	return false;
}

void Menu::deleteSelectedLink() {
#ifdef DEBUG
	cout << "\033[0;34mGMENU2X:\033[0m Deleting link " << selLink()->getTitle() << endl;
#endif
	if (selLinkApp()!=NULL)
		unlink(selLinkApp()->file.c_str());
	gmenu2x->sc.del(selLink()->getIconPath());
	sectionLinks()->erase( sectionLinks()->begin() + selLinkIndex() );
	setLinkIndex(selLinkIndex());
}

void Menu::deleteSelectedSection() {
#ifdef DEBUG
	cout << "\033[0;34mGMENU2X:\033[0m Deleting section " << selSection() << endl;
#endif
	gmenu2x->sc.del("sections/"+selSection()+".png");
	links.erase( links.begin()+selSectionIndex() );
	sections.erase( sections.begin()+selSectionIndex() );
	setSectionIndex(0); //reload sections
}

bool Menu::linkChangeSection(uint linkIndex, uint oldSectionIndex, uint newSectionIndex) {
	if (oldSectionIndex<sections.size() && newSectionIndex<sections.size() && linkIndex<sectionLinks(oldSectionIndex)->size()) {
		sectionLinks(newSectionIndex)->push_back( sectionLinks(oldSectionIndex)->at(linkIndex) );
		sectionLinks(oldSectionIndex)->erase( sectionLinks(oldSectionIndex)->begin()+linkIndex );
		//Select the same link in the new position
		setSectionIndex(newSectionIndex);
		setLinkIndex(sectionLinks(newSectionIndex)->size()-1);
		return true;
	}
	return false;
}

void Menu::linkLeft() {
	if (iLink%gmenu2x->linkColumns == 0)
		setLinkIndex( sectionLinks()->size()>iLink+gmenu2x->linkColumns-1 ? iLink+gmenu2x->linkColumns-1 : sectionLinks()->size()-1 );
	else
		setLinkIndex(iLink-1);
}

void Menu::linkRight() {
	if (iLink%gmenu2x->linkColumns == (gmenu2x->linkColumns-1) || iLink == (int)sectionLinks()->size()-1)
		setLinkIndex(iLink-iLink%gmenu2x->linkColumns);
	else
		setLinkIndex(iLink+1);
}

void Menu::linkUp() {
	int l = iLink-gmenu2x->linkColumns;
	if (l<0) {
		uint rows = (uint)ceil(sectionLinks()->size()/(double)gmenu2x->linkColumns);
		l = (rows*gmenu2x->linkColumns)+l;
		if (l >= (int)sectionLinks()->size())
			l -= gmenu2x->linkColumns;
	}
	setLinkIndex(l);
}

void Menu::linkDown() {
	uint l = iLink+gmenu2x->linkColumns;
	if (l >= sectionLinks()->size()) {
		uint rows = (uint)ceil(sectionLinks()->size()/(double)gmenu2x->linkColumns);
		uint curCol = (uint)ceil((iLink+1)/(double)gmenu2x->linkColumns);
		if (rows > curCol)
			l = sectionLinks()->size()-1;
		else
			l %= gmenu2x->linkColumns;
	}
	setLinkIndex(l);
}

int Menu::selLinkIndex() {
	return iLink;
}

Link *Menu::selLink() {
	if (sectionLinks()->size()==0) return NULL;
	return sectionLinks()->at(iLink);
}

LinkApp *Menu::selLinkApp() {
	return dynamic_cast<LinkApp*>(selLink());
}

void Menu::setLinkIndex(int i) {
	if (i<0)
		i=sectionLinks()->size()-1;
	else if (i>=(int)sectionLinks()->size())
		i=0;

	if (i>=(int)(iFirstDispRow*gmenu2x->linkColumns+gmenu2x->linkColumns*gmenu2x->linkRows))
		iFirstDispRow = i/gmenu2x->linkColumns-gmenu2x->linkRows+1;
	else if (i<(int)(iFirstDispRow*gmenu2x->linkColumns))
		iFirstDispRow = i/gmenu2x->linkColumns;

	iLink = i;
}

void Menu::readLinks() {
	vector<string> linkfiles;

	iLink = 0;
	iFirstDispRow = 0;

	DIR *dirp;
	struct stat st;
	struct dirent *dptr;
	string filepath;

	for (uint i=0; i<links.size(); i++) {
		links[i].clear();
		linkfiles.clear();

		if ((dirp = opendir(sectionPath(i).c_str())) == NULL) continue;

		while ((dptr = readdir(dirp))) {
			if (dptr->d_name[0]=='.') continue;
			filepath = sectionPath(i)+dptr->d_name;
			int statRet = stat(filepath.c_str(), &st);
			if (S_ISDIR(st.st_mode)) continue;
			if (statRet != -1) {
				linkfiles.push_back(filepath);
			}
		}

		sort(linkfiles.begin(), linkfiles.end(),case_less());
		for (uint x=0; x<linkfiles.size(); x++) {
			LinkApp *link = new LinkApp(gmenu2x, linkfiles[x].c_str());
			link->setSize(gmenu2x->skinConfInt["linkWidth"],gmenu2x->skinConfInt["linkHeight"]);
			if (link->targetExists())
				links[i].push_back( link );
			else
				free(link);
		}

		closedir(dirp);
	}
}
