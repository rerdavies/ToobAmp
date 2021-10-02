#pragma once

class OutputPort
{
private:
	float* pOut = 0;
	float defaultValue;
public:
	OutputPort(float defaultValue = 0)
	{
		this->defaultValue = defaultValue;
	}
	void SetData(void* data)
	{
		if (pOut != NULL)
		{
			defaultValue = *pOut;
		}
		pOut = (float*)data;
		if (pOut != NULL)
		{			*pOut = defaultValue;
		}
	}

	void SetValue(float value)
	{
		if (pOut != 0)
		{
			*pOut = value;
		}
		else {
			defaultValue = value;
		}
	}
};