class scrlog {
private:
int nrows;
unsigned short startrow;
char *rowbuf;
void clrbuf();
static void validateParms(int maxrows, unsigned short firstrow);
public:
scrlog(int maxrows, unsigned short firstrow);
virtual ~scrlog();
void addString(const char *st);
void reshape(int newmaxrows, unsigned short newfirstrow);
int getNrows()
{
    return nrows;
}


unsigned short getStartRow()
{
    return startrow;
}


void autoSize(int nfreq);
void redraw();
};
