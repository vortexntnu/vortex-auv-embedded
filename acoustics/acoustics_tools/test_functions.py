""" import unittest
import numpy as np
from functions import TDOA_pos_solve, adc_oversampling, single_freq_DFT, TDOA_calculate

class TestAcousticsFunctions(unittest.TestCase):

    def test_TDOA_pos_solve(self):
        # Test with simple known positions
        r = [np.array([0,0,0]), np.array([1,0,0]), np.array([0,1,0]), np.array([0,0,1])]
        c = 1500.0
        # Assume source at [0.5, 0.5, 0.5]
        s_true = np.array([0.5, 0.5, 0.5])
        t = [np.linalg.norm(s_true - ri) / c for ri in r]
        p = TDOA_pos_solve(r, t, c)
        np.testing.assert_allclose(p, s_true, atol=1e-2)  # Allow some tolerance

    def test_adc_oversampling(self):
        signal = np.array([1, 2, 3, 4, 5, 6])
        s = 2
        result = adc_oversampling(signal, s)
        expected = np.array([1.5, 3.5, 5.5])  # Averages of [1,2], [3,4], [5,6]
        np.testing.assert_array_equal(result, expected)

    def test_single_freq_DFT(self):
        # Test with a sine wave at freq=1, dt=1
        freq = 1.0
        dt = 1.0
        t = np.arange(0, 10, dt)
        signal = np.sin(2 * np.pi * freq * t)
        amplitude, phase = single_freq_DFT(signal, freq, dt)
        self.assertAlmostEqual(amplitude, 1.0, places=1)  # Should be close to 1
        self.assertAlmostEqual(phase, 0.0, places=1)  # Phase should be 0

    def test_TDOA_calculate(self):
        times = [0.0, 0.1, 0.2, 0.3]
        result = TDOA_calculate(times)
        expected = [0.1, 0.2, 0.3]
        self.assertEqual(result, expected)

if __name__ == '__main__':
    unittest.main() """