#include "DbDezipper.h"

using namespace TwoPlay;


static const int SEGMENT_SIZE = 64;
static const float DB_PER_SECOND = 9600.0; // 0...1 in 1/10 sec. 


void DbDezipper::SetSampleRate(double rate)
{
    this->dbPerSegment = DB_PER_SECOND*SEGMENT_SIZE/rate;

}
void DbDezipper::NextSegment()
{
    if (targetDb == currentDb)
    {
        x = targetX;
        dx = 0;
        if (targetDb <= -96)
        {
            x = 0;
        }
        count = -1;
        return;
    } else if (targetDb < currentDb)
    {
        currentDb -= dbPerSegment;
        if (currentDb < targetDb)
        {
            currentDb = targetDb;
        }
    } else {
        currentDb += dbPerSegment;
        if (currentDb > targetDb)
        {
            currentDb = targetDb;
        }
    }
    this->targetX = TwoPlay::Db2Af(currentDb);
    this->dx = (targetX-x)/SEGMENT_SIZE;
    this->count = SEGMENT_SIZE;

}