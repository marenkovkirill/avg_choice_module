#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "module.h"
#include "db_module.h"

#include "test_db_module.h"

/* GLOBALS CONFIG */

#define IID "RCT.Test_db_module_v101"

TestDBModule::TestDBModule() {
  mi = new ModuleInfo;
  mi->uid = IID;
  mi->mode = ModuleInfo::Modes::PROD;
  mi->version = BUILD_NUMBER;
  mi->digest = NULL;
}

const struct ModuleInfo &TestDBModule::getModuleInfo() { return *mi; }

void TestDBModule::prepare(colorPrintfModule_t *colorPrintf_p,
                              colorPrintfModuleVA_t *colorPrintfVA_p) {
  this->colorPrintf_p = colorPrintfVA_p;
}

void *TestDBModule::writePC(unsigned int *buffer_length) {
  (*buffer_length) = 0;
  return NULL;
}

int TestDBModule::init() {
  //connect to data base here
  return 0;
}

void TestDBModule::final() {
  //disconnect from data base here
}

int TestDBModule::startProgram(int uniq_index) { return 0; }

const DBRobotData *TestDBModule::makeChoise(const DBRobotData** robots_data, unsigned int count_robots) {
  if (!count_robots) {
    return NULL;
  }
  if (count_robots >= 3) {
    return robots_data[2];
  }
  return robots_data[count_robots-1];
}

int TestDBModule::endProgram(int uniq_index) { return 0; }

void TestDBModule::destroy() {
  delete mi;
  delete this;
}

void TestDBModule::colorPrintf(ConsoleColor colors, const char *mask, ...) {
  va_list args;
  va_start(args, mask);
  (*colorPrintf_p)(this, colors, mask, args);
  va_end(args);
}

PREFIX_FUNC_DLL unsigned short getDBModuleApiVersion() {
  return DB_MODULE_API_VERSION;
};
PREFIX_FUNC_DLL DBModule *getDBModuleObject() {
  return new TestDBModule();
}