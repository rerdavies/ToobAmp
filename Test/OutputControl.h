#pragma once

namespace TwoPlay {
	class OutputControl {
		friend class HostedLv2Plugin;
	private:
		float value;
	public:
		float GetValue()
		{
			return value;
		}
	private:
		float* GetLv2Data()
		{
			return &value;
		}
	};
}