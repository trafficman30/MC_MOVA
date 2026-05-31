#pragma once


class DataManagerForTesting
{
public:
	DataManagerForTesting(void);
	~DataManagerForTesting(void);

public: 
	static void ResetingMovaDataSet();
	static void ResetingMovaDynamicData();

};