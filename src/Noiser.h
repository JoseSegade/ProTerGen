#pragma once

#include <array>
#include <numeric>
#include <random>
#include <algorithm>

namespace ProTerGen
{
	class Noiser
	{
	public:
	public:
		Noiser() = default;
		virtual ~Noiser() = default;

		virtual double FBM(double x, double y, size_t octaves, double frecuency, double gain) const;
		virtual double FBM_turbulence(double x, double y, size_t octaves, double frecuency, double gain) const;

		virtual void Generate(uint32_t seed = 0) = 0;
		virtual double Noise(double x, double y) const = 0;
	};

	// Credit: https://cs.nyu.edu/~perlin/noise/
	class PerlinNoise : public Noiser
	{
	public:
		PerlinNoise() = default;
		virtual ~PerlinNoise() = default;

		void Generate(uint32_t seed = 0) override;
		double Noise(double x, double y) const override;
		double Noise(double x, double y, double z) const;

	protected:
		constexpr double Fade(double t) const;
		constexpr double Lerp(double t, double a, double b) const;
		constexpr double Grad(size_t hash, double x, double y, double z) const;

		std::array<uint32_t, 512> mPermutation{};
	};

	// Credit: https://iquilezles.org/articles/voronoise/
	class VoronoiNoise : public Noiser
	{
	public:
		VoronoiNoise() = default;
		virtual ~VoronoiNoise() = default;

		void Generate(uint32_t seed = 0) override;
		double Noise(double x, double y) const override;

		void SetRandomness(double randomness);
		void SetSmoothness(double smoothness);
	private:
		struct Point
		{
			double X = 0.0;
			double Y = 0.0;
			double Val = 0.0;
		};

		Point Hash(double x, double y) const;

		double mRandomness = 1.0;
		double mSmoothness = 1.0;
	};
}