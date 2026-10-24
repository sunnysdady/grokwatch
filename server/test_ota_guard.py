import os, sys, tempfile, unittest
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ota_guard import OtaGuard

def body(**boot):
    return {"board": {"type": "m5stack-stopwatch", "gw_boot": boot}}

class T(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp(); self.p = os.path.join(self.d, "data", "ota_guard.json")
        self.g = OtaGuard(self.p)
    def test_success_path_not_blocked(self):
        self.g.observe("A", "2.6.5", body(pv_failed=1)); self.assertTrue(self.g.allow("A", "2.6.6"))
        self.g.record_offer("A", "2.6.6"); self.g.observe("A", "2.6.6", body(pv_failed=1))
        self.assertTrue(self.g.allow("A", "2.6.7"))
    def test_reported_bad_ver_blocks(self):
        self.g.observe("A", "2.6.5", body(bad_ver="2.6.6", pv_failed=1))
        self.assertFalse(self.g.allow("A", "2.6.6")); self.assertTrue(self.g.allow("A", "2.6.7"))
        self.assertTrue(self.g.allow("B", "2.6.6"))  # per device
    def test_pv_failed_increase_blocks_offered(self):
        self.g.observe("A", "2.6.5", body(pv_failed=1)); self.g.record_offer("A", "2.6.6")
        self.g.observe("A", "2.6.5", body(pv_failed=2)); self.assertFalse(self.g.allow("A", "2.6.6"))
    def test_old_firmware_without_bootrec_capped(self):
        for _ in range(3):
            self.g.observe("A", "2.5.3", {}); self.assertTrue(self.g.allow("A", "2.6.6")); self.g.record_offer("A", "2.6.6")
        self.g.observe("A", "2.5.3", {}); self.assertFalse(self.g.allow("A", "2.6.6"))
    def test_persistence_and_corrupt_file(self):
        self.g.observe("A", "2.6.5", body(bad_ver="2.6.6"))
        self.assertFalse(OtaGuard(self.p).allow("A", "2.6.6"))
        open(self.p, "w").write("{not json"); self.assertTrue(OtaGuard(self.p).allow("A", "2.6.6"))
    def test_garbage_body(self):
        for b in (None, [], {"board": "x"}, {"board": {"gw_boot": "x"}}, body(bad_ver=5, pv_failed="x")):
            self.g.observe("A", "2.6.5", b)
        self.assertTrue(self.g.allow("A", "2.6.6"))

if __name__ == "__main__":
    unittest.main()
