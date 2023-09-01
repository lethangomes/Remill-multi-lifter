#pragma once
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
using namespace std;

vector<string> extract_instructions(ifstream& inputFile)
{
    vector<string> out;
    ofstream outFile("extracted_instructions.txt");
    bool introductionFound = false;

    //find where instructions start
    string currentLine;
    while(getline(inputFile, currentLine))
    {
        if(currentLine.find("Instructions") != string::npos)
        {
            introductionFound = true;
            break;
        }
    }

    //if the "Instructions" line is not found reset getline
    if(!introductionFound)
    {
        inputFile.clear();
        inputFile.seekg (0, ios::beg);
    }

    //extract instructions
    while(getline(inputFile, currentLine))
    {
        if(currentLine.find("RelocInfo") == string::npos)
        {
            //every line with instructions on it starts with 0x
            if(currentLine.substr(0,2) == "0x")
            {
                //cut out the relevant instructions
                currentLine = currentLine.substr(22);
                out.push_back(currentLine.substr(0, currentLine.find(" "))+ " \r");
                outFile << currentLine.substr(0, currentLine.find(" "))+ " \r";
            }
            else
            {
                continue;
            }
            
        }
        else
        {
            //end loop once we reach last line of instructions
            break;
        }
        
    }

    return out;
}
