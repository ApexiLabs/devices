"""Real signature integration; run in requirements-production-verification environment."""
import contextlib
import io
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_signed_release as fixtures
import espsecure
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa


class SignatureIntegrationTests(unittest.TestCase):
    def test_real_signature_wrong_key_and_tampering(self):
        fixture = fixtures.SignedReleaseTests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        # Ephemeral test-only material remains in memory, never in a file/log.
        key = rsa.generate_private_key(public_exponent=65537, key_size=3072)
        private_pem = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                        serialization.NoEncryption())
        fixture.key.write_bytes(key.public_key().public_bytes(serialization.Encoding.PEM,
                                                              serialization.PublicFormat.SubjectPublicKeyInfo))
        for name in fixtures.release.LIMITS:
            unsigned = (fixture.first / name).read_bytes()
            padded = unsigned + b"\xff" * (-len(unsigned) % 4096)
            with contextlib.redirect_stdout(io.StringIO()):
                block = espsecure.generate_signature_block_using_private_key([io.BytesIO(private_pem)], padded)
            (fixture.signed / name).write_bytes(padded + block.ljust(4096, b"\xff"))
        result = fixture.verify(fixtures.release.verify_signature)
        self.assertTrue(result["signed_artifacts"]["firmware.bin"]["signature_verified"])
        image = fixture.signed / "firmware.bin"
        damaged = bytearray(image.read_bytes())
        damaged[-4096 + 50] ^= 1
        image.write_bytes(damaged)
        with self.assertRaises(fixtures.release.EvidenceError): fixture.verify(fixtures.release.verify_signature)
        damaged[-4096 + 50] ^= 1
        image.write_bytes(damaged)
        wrong = rsa.generate_private_key(public_exponent=65537, key_size=3072)
        fixture.key.write_bytes(wrong.public_key().public_bytes(serialization.Encoding.PEM,
                                                               serialization.PublicFormat.SubjectPublicKeyInfo))
        with self.assertRaises(fixtures.release.EvidenceError): fixture.verify(fixtures.release.verify_signature)


if __name__ == "__main__": unittest.main()
