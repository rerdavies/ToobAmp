/*
 *   Copyright (c) 2022 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

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