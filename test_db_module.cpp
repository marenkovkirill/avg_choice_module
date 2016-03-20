#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sqlite3.h>
#include <string>

#include "module.h"
#include "db_module.h"

#include "test_db_module.h"

/* GLOBALS CONFIG */

#define IID "RCT.Test_db_module_v101"
typedef unsigned int uint;

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
  int rc;  
  rc = sqlite3_open("stats.db", &db);
  if( rc ){
    sqlite3_close(db);
    return(1);
  }  
  return 0;
}

void TestDBModule::final() {
  //disconnect from data base here
  sqlite3_close(db);  
}

int TestDBModule::startProgram(int uniq_index) { return 0; }

const DBRobotData *TestDBModule::makeChoise(const DBFunctionData** function_data, uint count_functions,
                                            const DBRobotData** robots_data, uint count_robots) {
  char *zErrMsg = 0;
  int nRow;
  int nCol;
  bool havenulluid;
  char **pRes;
  string *psqlText = new string[4];
  string sTmp;
  const char *psqlText2;
      
    // main + funcs
    sTmp = (string)"";
    psqlText[0] = 
    "select uid\n"
    "from robot_uids ru\n"
    "join\n"
    "(select fc.robot_id,\n"
    " avg(fc.end-fc.start) as avg_time\n"
    " from function_calls fc\n"
    " where exists(select 1\n"
    "              from functions f\n"
    "              join contexts c\n"
    "                   on f.context_id = c.id\n"
    "              where f.id = fc.function_id\n"
    "              and ( %WHERE_CLAUSE%\n"
    "                   )\n"
    "              )\n";
    sTmp = "(f.name = '" + (string)(function_data[0] -> name) + "'\n"
            "and f.position = " + to_string(function_data[0] -> position) + "\n"
            "and c.hash = '" + (string)(function_data[0] -> context_hash) + "')\n";
    for (uint i = 1; i <= count_functions-1; i++) {
         sTmp = sTmp + "or (f.name = '" + (string)(function_data[i] -> name) + "'\n"
                       "and f.position = " + to_string(function_data[i] -> position) + "\n"
                       "and c.hash = '" + (string)(function_data[i] -> context_hash) + "')\n";
    }
    psqlText[0].replace(psqlText[0].find("%WHERE_CLAUSE%"),14,sTmp);
        
    // robots
    psqlText[1] = (string)"";
    sTmp = (string)"";
    if (count_robots) {
        for (uint i = 0; i <= count_robots - 1; i++) {
            if (*robots_data[i] -> robot_uid == 0) {
                havenulluid = true;
                break;
            } else {
                sTmp += (string)"'" + (string)(robots_data[i] -> robot_uid) + "',";
            }
        }
        if (not havenulluid) {
            psqlText[1] =
            "and exists (select 1 from robot_uids ru\n"
            "		where ru.id = fc.robot_id\n"
            "		and ru.uid in (%IN_CLAUSE%))\n";
            sTmp = sTmp.substr(0,sTmp.length()-1);
            psqlText[1].replace(psqlText[1].find("%IN_CLAUSE%"),11,sTmp);
        }
    }
    
    // group by
    psqlText[2] =        
    "group by fc.robot_id\n"
    "order by avg_time asc\n"
    "limit 1\n"    
    ") b\n"
    "on (ru.id = b.robot_id)\n";
    
    // concat
    for (uint i = 0; i <= 2; i++) {
        psqlText[3] += psqlText[i];
    }
    
    // string to char[]    
    psqlText2 = psqlText[3].c_str();
    
    if( sqlite3_get_table(db, psqlText2, &pRes, &nRow, &nCol, &zErrMsg) != SQLITE_OK ){
       sqlite3_free(zErrMsg);
       return NULL;
    }
    
    if (nCol > 0) {
    for (uint i = 0; i <= count_robots - 1; i++) {
        if ( (string)(robots_data[i] -> robot_uid) == (string)pRes[1]) {
            sqlite3_free_table(pRes);
            return robots_data[i];
            }
        }
    }
       
    sqlite3_free_table(pRes);
    return NULL;
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
  return MODULE_API_VERSION;
};
PREFIX_FUNC_DLL DBModule *getDBModuleObject() {
  return new TestDBModule();
}
