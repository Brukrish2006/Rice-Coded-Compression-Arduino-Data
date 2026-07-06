import unittest
from decompress import RiceDecoder, zigzag_decode

class TestDecompress(unittest.TestCase):
    def test_zigzag_decode(self):
        self.assertEqual(zigzag_decode(0), 0)
        self.assertEqual(zigzag_decode(1), -1)
        self.assertEqual(zigzag_decode(2), 1)
        self.assertEqual(zigzag_decode(3), -2)
        self.assertEqual(zigzag_decode(4), 2)

    def test_rice_decoder_normal(self):
        # Let's say k = 2
        # v = 6
        # q = 6 >> 2 = 1
        # r = 6 & 3 = 2
        # Unary: 10 (q=1)
        # Binary: 10 (r=2)
        # Bits: 1010
        decoder = RiceDecoder(2)
        value, next_idx = decoder.decode_value("1010", 0)
        self.assertEqual(value, 6)
        self.assertEqual(next_idx, 4)

    def test_rice_decoder_escape(self):
        # Let's say k = 2, ESCAPE_THRESHOLD = 127
        # v = 200 (which is > 127)
        # esc_q = 127 >> 2 = 31
        # Unary part: 31 ones + 1 zero
        # Binary part: 16 bits for value 200 -> 0000 0000 1100 1000
        decoder = RiceDecoder(2)
        bits = "1" * 31 + "0" + format(200, '016b')
        value, next_idx = decoder.decode_value(bits, 0)
        self.assertEqual(value, 200)
        self.assertEqual(next_idx, len(bits))

if __name__ == '__main__':
    unittest.main()
