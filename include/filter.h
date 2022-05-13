class Filter
{
private:
int prevValue;
int hitCount;
int limit;
public:
Filter(int filterCount) : prevValue(0), hitCount(0), limit(filterCount)
{
}


int filter(int newVal)
{
    if (newVal == prevValue)
    {
        if (hitCount == limit || ++hitCount == limit)
            return newVal;
        else
            return 0;
    }
    else
    {
        prevValue = newVal;
        hitCount = 1;
        return 0;
    }
}


};
