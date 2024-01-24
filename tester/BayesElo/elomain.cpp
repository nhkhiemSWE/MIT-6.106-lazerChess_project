// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

/////////////////////////////////////////////////////////////////////////////
//
// Rémi Coulom
//
// December, 2004
//
/////////////////////////////////////////////////////////////////////////////
#include <iostream>  // NOLINT(readability/streams)
#include <string>
#include <vector>

#include "./version.h"
#include "./CResultSet.h"
#include "./CResultSetCUI.h"

/////////////////////////////////////////////////////////////////////////////
// main function
/////////////////////////////////////////////////////////////////////////////
int main() {
  std::cout << CVersion::GetCopyright();
  CResultSet rs;
  std::vector<std::string> vecName;
  CResultSetCUI rscui(rs, vecName);
  rscui.MainLoop(std::cin, std::cout);

  return 0;
}
