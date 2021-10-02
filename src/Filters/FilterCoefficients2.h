#pragma once


namespace TwoPlay {
	class FilterCoefficients2 {
	public:
		FilterCoefficients2()
		{
			a[0] = a[1] = a[2] = 0;
			b[0] = b[1] = b[2] = 0;
		}
		FilterCoefficients2(double b0, double b1, double b2,double a0, double a1, double a2)
		{
			a[0] = a0; a[1] = a1; a[2] = a2;
			b[0] = b0; b[1] = b1; b[2] = b2;
		}
		FilterCoefficients2(const FilterCoefficients2& other)
		{
			a[0] = other.a[0];
			a[1] = other.a[1];
			a[2] = other.a[2];
			b[0] = other.b[0];
			b[1] = other.b[1];
			b[2] = other.b[2];
		}
		void Disable()
		{
			a[0] = b[0] = 1;
			a[1] = a[2] = b[1] = b[2] = 0;
		}

		double a[3];
		double b[3];
	};
}