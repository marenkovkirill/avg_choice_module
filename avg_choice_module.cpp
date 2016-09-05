/* INCLUDE CONFIG */
/* General */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sqlite3.h>

#include <string>
#include <vector>

#ifdef _WIN32
//#include "stringC11.h"
#endif

#include "SimpleIni.h"

/* RCML */
#include "module.h"
#include "choice_module.h"
#include "avg_choice_module.h"

/* GLOBALS CONFIG */
#define IID "RCT.AVG_choise_module_v101"
typedef unsigned int uint;

AvgChoiceModule::AvgChoiceModule() {
  mi = new ModuleInfo;
  mi->uid = IID;
  mi->mode = ModuleInfo::Modes::PROD;
  mi->version = BUILD_NUMBER;
  mi->digest = NULL;
}

const struct ModuleInfo &AvgChoiceModule::getModuleInfo() { return *mi; }

void AvgChoiceModule::prepare(colorPrintfModule_t *colorPrintf_p,
                              colorPrintfModuleVA_t *colorPrintfVA_p) {
  this->colorPrintf_p = colorPrintfVA_p;
}

void *AvgChoiceModule::writePC(unsigned int *buffer_length) {
  (*buffer_length) = 0;
  return NULL;
}

int AvgChoiceModule::init() {
  std::string config_path = "config.ini";

  CSimpleIniA ini;
  ini.SetMultiKey(true);

  if (ini.LoadFile(config_path.c_str()) < 0) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "Can't load '%s' file!",
                config_path.c_str());
    return 1;
  }

  db_path = ini.GetValue("statisctic", "db_path", NULL);
  if (db_path.empty()) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "Can't read db_path value");
    return 1;
  }

  sqlite3 *db;
  int rc = sqlite3_open(db_path.c_str(), &db);
  if (rc) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "Can't open database: %s\n",
                sqlite3_errmsg(db));
    sqlite3_close(db);
    return (1);
  }
  sqlite3_close(db);

  return 0;
}

void AvgChoiceModule::final() {}

int AvgChoiceModule::readPC(int pc_index, void *buffer,
                            unsigned int buffer_length) {
  return 0;
}

int AvgChoiceModule::startProgram(int run_index, int pc_index) { return 0; }

const ChoiceRobotData *AvgChoiceModule::makeChoice(int run_index,
    const ChoiceFunctionData **function_data, uint count_functions,
    const ChoiceRobotData **robots_data, uint count_robots) {

  if (not ((count_functions == 1) && count_robots)) {
     return NULL;
  }

  sqlite3 *db;
  int rc = sqlite3_open(db_path.c_str(), &db);
  if (rc) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "Can't open database: %s\n",
                sqlite3_errmsg(db));
    sqlite3_close(db);
    return NULL;
  }

  string sqlQuery = 
    "select\n"     
    "  s.iid,\n"
    "  ru.uid,\n"
    "avg(fc.end - fc.start) as avg_time\n"
    "from\n"
    "  function_calls as fc,\n"
    "  robot_uids as ru,\n"
    "  sources as s,\n"
    "  functions as f,\n"
    "  contexts as c\n"
    "where\n"
    "  fc.function_id = f.id\n"
    "  and f.context_id = c.id\n"
    "  and fc.robot_id = ru.id\n"
    "  and ru.source_id = s.id\n"

    "  /* calls restrict */\n"
    "  and fc.success = 1\n"
    "  and fc.end is not NULL\n";


  string functionsRestrict = "and ";
  const ChoiceFunctionData *func_data = function_data[0];

  functionsRestrict += "f.name = '" + (string)(func_data->name) + 
                        "'\nand " +
                        "c.hash = '" + (string)(func_data->context_hash) + 
                        "'\n";

  map<string, vector<string>> modulesRobots;
  bool isEmptyName = false;
  for (uint i = 0; i < count_robots; ++i) {
    const char* checkUid = robots_data[i]->robot_uid;
    
    if (checkUid != NULL) {
      string uid(checkUid);
      if (uid.empty()){
        isEmptyName = true;
        break;
      }
      const char* checkIid = robots_data[i]->module_data->iid;
      string tmpIid(checkIid);

      colorPrintf(ConsoleColor(ConsoleColor::yellow), "%d iid: %s\n",
        i, tmpIid.c_str());

      if (modulesRobots.count(tmpIid)) {
        modulesRobots[tmpIid].push_back(uid);
      } else {
        vector<string> newVector;
        newVector.push_back(uid);
        modulesRobots[tmpIid] = newVector;
      }
    } else {
      isEmptyName = true;
      break;
    }
  }

  
  string robotsRestrict("");

  if (!isEmptyName) {              
    for (auto robots = modulesRobots.begin(); robots != modulesRobots.end(); ++robots) {

      string robotsUid("");
      auto robotsNames = robots->second;
      for (uint i = 0; i < robotsNames.size(); ++i) {
        if (!robotsUid.empty()) {
          robotsUid += ",";
        }
        robotsUid += "'" + robotsNames[i] + "'";
      }

      robotsUid = "(" + robotsUid + ")";

      if (!robotsRestrict.empty()) {
        robotsRestrict += "or ";
      }

      robotsRestrict += string("(\n") + 
                         "s.iid = '" + robots->first + "'\nand " +
                         "s.type = 2\nand " +
                         "ru.uid in" + robotsUid + "\n)\n";
    }

    robotsRestrict = "and (\n" + robotsRestrict + ")\n";
  }

  sqlQuery += functionsRestrict + robotsRestrict + 
               "group by s.iid,ru.uid order by avg_time ASC";

#ifdef IS_DEBUG
  colorPrintf(ConsoleColor(ConsoleColor::yellow), "SQL statement:\n%s\n\n",
              sqlQuery.c_str());
#endif

  int num_row = 0;
  int num_col = 0;
  char *errorMessage = 0;
  char **sqlResult = 0;
  if (sqlite3_get_table(db, sqlQuery.c_str(), &sqlResult, &num_row, &num_col,
                        &errorMessage) != SQLITE_OK) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "SQL error:\n%s\n\n",
                errorMessage);
    sqlite3_free(errorMessage);
    sqlite3_close(db);
    return NULL;
  }

  const ChoiceRobotData *resultRobot = NULL;
  std::vector<ResultData> resultData;

  for (int i = 0; i < num_row; ++i) {
    const int iidPos = (i+1)*num_col;
    const int uidPos = iidPos + 1;
    const int avgTimePos = uidPos + 1;

    const string iid(sqlResult[iidPos]);
    const string uid(sqlResult[uidPos]);
    const string averageTime(sqlResult[avgTimePos]);

    resultData.push_back(ResultData(iid, uid, stod(averageTime)));
  }

  bool isRobotFinded = false;
  for (uint i = 0; i < count_robots; i++) {
    const ChoiceRobotData *data = robots_data[i];

    const string currentIid(data->module_data->iid);
    const string currentUid(data->robot_uid);

    isRobotFinded = false;
    for (uint resIndex = 0; resIndex < resultData.size(); ++resIndex) {
      if (!currentIid.compare(resultData[resIndex].iid) && 
          !currentUid.compare(resultData[resIndex].uid)){
        isRobotFinded = true;
        break;
      }
    }

    if (!isRobotFinded){
      colorPrintf(ConsoleColor(ConsoleColor::green), "ADD Robot\n");
      resultRobot = data;
      break;
    }
  }

  if (isRobotFinded){
    for (uint i = 0; i < count_robots; i++) {
      const ChoiceRobotData *data = robots_data[i];
      string currentIid(data->module_data->iid);
      string currentUid(data->robot_uid);
  
      if (!currentIid.compare(resultData[0].iid) &&
          !currentUid.compare(resultData[0].uid)){
        resultRobot = data;
      break;
      }
    }
  }

#ifdef IS_DEBUG
  string result = "NULL";
  if (resultRobot != NULL) {
    result = (string)(resultRobot->robot_uid);
    result += " : ";
    result += (string)(resultRobot->module_data->iid);
  }
  colorPrintf(ConsoleColor(ConsoleColor::yellow), "MakeChoice result:\n%s\n\n",
              result.c_str());
#endif
  sqlite3_free_table(sqlResult);
  sqlite3_close(db);

  return resultRobot;
}

int AvgChoiceModule::endProgram(int run_index) { return 0; }

void AvgChoiceModule::destroy() {
  delete mi;
  delete this;
}

void AvgChoiceModule::colorPrintf(ConsoleColor colors, const char *mask, ...) {
  va_list args;
  va_start(args, mask);
  (*colorPrintf_p)(this, colors, mask, args);
  va_end(args);
}

PREFIX_FUNC_DLL unsigned short getChoiceModuleApiVersion() {
  return MODULE_API_VERSION;
}
PREFIX_FUNC_DLL ChoiceModule *getChoiceModuleObject() {
  return new AvgChoiceModule();
}
