#include <malloc.h>

void test_cache_hit_invalidate_alias(TestContext *ctx) {
	uint8_t *buf = memalign(16, 64);
	ASSERT(buf != NULL, "cache test allocation failed");
	DEFER(free(buf));

	disable_interrupts();
	DEFER(enable_interrupts());
	DEFER(data_cache_hit_invalidate(buf, 64));

	// Internal diagnostic state must not be acknowledged by a cache operation.
	extern bool __expanded_memory_asserted;
	bool saved_expanded_memory_asserted = __expanded_memory_asserted;
	DEFER(__expanded_memory_asserted = saved_expanded_memory_asserted);

	volatile uint8_t *cached = buf;
	volatile uint8_t *uncached = UncachedAddr(buf);
	for (int alias=0; alias<2; alias++) {
		for (int zero=0; zero<2; zero++) {
			for (int acknowledged=0; acknowledged<2; acknowledged++) {
				data_cache_hit_invalidate(buf, 64);
				for (int i=0; i<64; i++) uncached[i] = 0;
				MEMORY_BARRIER();
				// Dirty all four lines, then replace RAM without touching cache.
				for (int i=0; i<64; i++) cached[i] = 0xA0 + i;
				for (int i=0; i<64; i++) uncached[i] = 0x20 + i;
				MEMORY_BARRIER();

				__expanded_memory_asserted = acknowledged;
				data_cache_hit_invalidate(alias ? uncached+16 : cached+16, zero ? 0 : 32);
				MEMORY_BARRIER();
				bool acknowledged_after = __expanded_memory_asserted;

				// Collect results before reporting: assertion logging can evict lines.
				bool ram_ok = true, cache_ok = true;
				for (int i=0; i<64; i++) {
					if (uncached[i] != 0x20 + i) ram_ok = false;
					bool invalidated = !zero && i >= 16 && i < 48;
					uint8_t expected = (invalidated ? 0x20 : 0xA0) + i;
					if (cached[i] != expected) cache_ok = false;
				}
				ASSERT(ram_ok, "invalidate wrote back dirty data (alias=%d zero=%d)", alias, zero);
				ASSERT(cache_ok, "target/neighbor cache mismatch (alias=%d zero=%d)", alias, zero);
				ASSERT(acknowledged_after == acknowledged,
					"invalidate changed expansion acknowledgement (alias=%d zero=%d)", alias, zero);
			}
		}
	}
}

void test_cache_invalidate(TestContext *ctx) {
	// Interrupts causing other code to run can easily invalidate cache and make
	// this test useless. So we need them disabled while running this.
	disable_interrupts();
	DEFER(enable_interrupts());

	const int BUF_SIZE = 64;
	uint8_t *buf = memalign(16, BUF_SIZE);
	DEFER(free(buf));

	pi_addr_t rom = dfs_rom_addr("counter.dat");
	ASSERT(rom != 0, "counter.dat not found");

	// First 32 bytes of counter.dat (incrementing sequence).
	static const uint8_t counter32[32] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	};
	const char *aaa = "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa";

	for (int i=0;i<32;i++) {
		for (int j=0;j<32;j++) {

			// Read/write the whole buffer through cache,
			// so it's all populated in D-Cache.
			memset(buf, 0xA0, BUF_SIZE);
			for (int i=0;i<BUF_SIZE;i++) buf[i] += 0xA;

			// Writeback+Invalidate buf[i..i+j]. Now only
			// those lines should be invalidated.
			data_cache_hit_writeback_invalidate(buf+i, j);
			
			// Read from ROM (counter.dat)
			dma_read(UncachedAddr(buf), rom, 32);
			dma_read(UncachedAddr(buf+32), rom, 32);

			// For each cache-line, check whether the contents
			// match what we would expect from it:
			//  * Invalidated: we should see the data read from ROM
			//  * Not invalidated: we should see the 0xAA fill in the cache
			for (int c=0;c<4;c++) {
				bool should_be_invalidated = (j!=0) && (c >= i/16 && c <= (i+j-1)/16);

				// NOTE: ASSERT_EQUAL_MEM would do the byte-by-byte check for us
				// but it does touch the stack a lot, and that can invalidate
				// cachelines. We do the check here inline so that we don't
				// touch memory. If there's a failure, we can fallback on
				// ASSERT_EQUAL_MEM to provide error reporting.
				if (should_be_invalidated) {
					for (int k=0;k<16;k++) {
						if (buf[c*16+k] != counter32[c%2*16+k]) {
							ASSERT_EQUAL_MEM(buf+c*16, counter32+c%2*16, 16,
								"unexpected data in invalidated cacheline %d (%d/%d)", c, i, j);
						}
					}
				} else {
					for (int k=0;k<16;k++) {
						if (buf[c*16+k] != (uint8_t)aaa[k]) {
							ASSERT_EQUAL_MEM(buf+c*16, (uint8_t*)aaa, 16,
								"unexpected data in not-invalidated cached cacheline %d (%d/%d)", c, i, j);
						}
					}
				}
			}
		}
	}
}
