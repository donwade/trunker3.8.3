template <class T> class ptrMap
{
public:
ptrMap() : numblocks(256), numperblock(256), numEntries(0)
{
	baseMem = (T***)calloc(numblocks, sizeof(void *));
	if (!baseMem)
		shouldNever("can't get base block for map");
}
virtual ~ptrMap()
{
	clear();
	if ( baseMem )
	{
		register int i;
		for (i = 0; i < numblocks; ++i)
			if ( baseMem[i] )
				free(baseMem[i]);
		free(baseMem);
	}
}
void insert(unsigned short k, T * v)
{
	if (!v)
		shouldNever("attempt to insert null pointer into map");
	T ** x = getAblock((unsigned short)(k / numperblock));
	T * xx = *(x + (k % numperblock));
	if ( xx && (xx != v))
		shouldNever("attempt to overwrite map entry!");
	*(x + (k % numperblock)) = v;
	++numEntries;
}

void remove(unsigned short k)
{
	T ** x = getAblock((unsigned short)(k / numperblock));
	T * xx = x[k % numperblock];

	if ( !xx )
		shouldNever("attempt to remove non-existent map entry");
	x[k % numperblock] = 0;
	--numEntries;
}

BOOL contains(unsigned short k)
{
	return find(k) ? 1 : 0;
}

T * find(unsigned short k)
{
	T ** x = baseMem[k / numperblock];

	if (x)
	{
		T * xx = *(x + (k % numperblock));
		return xx;
	}
	else
		return 0;
}

void freeAll()
{
	if (numEntries)
	{
		numEntries = 0;
		register int i, j;
		register T ** tp;
		for (i = 0; i < numblocks; ++i)
		{
			if ( baseMem[i] )
			{
				for (j = 0, tp = baseMem[i]; j < numperblock; ++j, ++tp)
					if ( *tp )
						delete *tp;
				memset(baseMem[i], 0, sizeof(void *) * numperblock);
			}
		}
	}
}

int entries() const
{
	return numEntries;
}

void clear()
{
	if (numEntries)
	{
		numEntries = 0;
		register int i;
		for (i = 0; i < numblocks; ++i)
			if ( baseMem[i] )
				memset(baseMem[i], 0, sizeof(void *) * numperblock);
	}
}

BOOL isEmpty() const
{
	return numEntries ? FALSE : TRUE;
}

void forAll(void (* fcn)(T *, void *), void * parm) const
{
	if (numEntries)
	{
		register int i, j;
		register T ** tp;
		for (i = 0; i < numblocks; ++i)
		{
			if ( baseMem[i] )
			{
				for (j = 0, tp = baseMem[i]; j < numperblock; ++j, ++tp)
					if ( *tp )
						(*fcn)(*tp, parm);
			}
		}
	}
}
private:
T ** getAblock(unsigned short i)
{
	T ** x = baseMem[i];

	if ( !x )
	{
		x = (T**)calloc(numperblock, sizeof(void *));
		if (!x)
			shouldNever("can't allocate block of pointers for map");
		baseMem[i] = x;
	}
	return x;
}
T *** baseMem;
int numblocks;
unsigned short numperblock;
unsigned short numEntries;
};

