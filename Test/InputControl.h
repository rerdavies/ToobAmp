#pragma once

namespace TwoPlay
{

	class InputControl {
		friend class HostedLv2Plugin;
		float value;
	public:
		InputControl(float initialValue = 0)
		{
			this->value = initialValue;
		}

		virtual void SetValue(float value)
		{
			this->value = value;
		}

		float GetValue()
		{
			return this->value;
		}

	private:
		float* GetLv2Data() {
			return &value;
		}
	};

	class RangedInputControl : public InputControl {
	private:
		float minValue, maxValue;
	public:
		RangedInputControl(float initialValue, float minValue, float maxValue)
			:InputControl(initialValue)
		{
			this->minValue = minValue;
			this->maxValue = maxValue;
		}

		virtual void SetValue(float value)
		{
			if (value < minValue)
			{
				value = minValue;
			}
			if (value > maxValue)
			{
				value = maxValue;
			}
			InputControl::SetValue(value);
		}
	};
}