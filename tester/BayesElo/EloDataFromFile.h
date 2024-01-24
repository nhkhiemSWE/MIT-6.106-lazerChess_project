// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

/////////////////////////////////////////////////////////////////////////////
//
// Rémi Coulom
//
// December, 2004
//
/////////////////////////////////////////////////////////////////////////////
#ifndef EloDataFromFile_Declared
#define EloDataFromFile_Declared

#include <vector>
#include <string>

class CPGNLex;
class CResultSet;

void EloDataFromFile(CPGNLex& pgnlex,
                     CResultSet& rs,
                     std::vector<std::string>& vNames);

#endif  // EloDataFromFile_Declared
