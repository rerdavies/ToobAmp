#pragma once


namespace TwoPlay {
	class FilterCoefficients3 {
	public:
		FilterCoefficients3()
		{
			a[0] = a[1] = a[2] = a[3] = 0; 
			b[0] = b[1] = b[2] = b[3] = 0; 
		}
		FilterCoefficients3(double b0, double b1, double b2,double b3,double a0, double a1, double a2,double a3)
		{
			a[0] = a0; a[1] = a1; a[2] = a2; a[3] = a3;
			b[0] = b0; b[1] = b1; b[2] = b2; b[3] = b3;
		}
		FilterCoefficients3(const FilterCoefficients3& other)
		{
			a[0] = other.a[0];
			a[1] = other.a[1];
			a[2] = other.a[2];
			a[3] = other.a[3];
			b[0] = other.b[0];
			b[1] = other.b[1];
			b[2] = other.b[2];
			b[3] = other.b[3];
		}
		void Disable()
		{
			a[0] = b[0] = 1;
			a[1] = a[2] = a[3] = b[1] = b[2] = b[3] = 0;
		}

		double a[4];
		double b[4];
	};
}