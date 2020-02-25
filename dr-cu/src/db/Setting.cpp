#include "Setting.h"
#include "Database.h"

namespace db {

void Setting::makeItSilent() {
    singleNetVerbose = VerboseLevelT::LOW;
    multiNetVerbose = VerboseLevelT::LOW;
    dbVerbose = VerboseLevelT::LOW;
}

void Setting::adapt(Database const& database) {
    if (database.nets.size() < 10000) {
        ++rrrIterLimit;
    }
    else if (database.nets.size() > 800000) {
        --rrrIterLimit;
    }
}

void RrrIterSetting::update(Database const& database, int iter) {
    if (iter == 0) {
        defaultGuideExpand = database.setting().defaultGuideExpand;
        wrongWayPointDensity = database.setting().wrongWayPointDensity;
        addDiffLayerGuides = false;
    } else {
        defaultGuideExpand += iter * 2;
        wrongWayPointDensity = std::min(1.0, wrongWayPointDensity + 0.1);
        if (database.nets.size() < 200000) {
            // high-effort mode (exclude million-net test case)
            addDiffLayerGuides = true;
        }
    }
    converMinAreaToOtherVio = ((iter + 1) < database.setting().rrrIterLimit);
}

void RrrIterSetting::print() const {
    printlog("defaultGuideExpand =", defaultGuideExpand);
    printlog("wrongWayPointDensity =", wrongWayPointDensity);
    printlog("addDiffLayerGuides =", addDiffLayerGuides);
}

}  // namespace db
