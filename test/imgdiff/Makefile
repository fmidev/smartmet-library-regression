PROG := ../../smartimagediff_psnr

test: test-identical test-similar test-unrelated

test-identical:
	@echo "Testing identical images (should report PSNR: inf)..."
	$(PROG) 1.png 1.png

test-similar:
	@echo "Testing similar images (should report a finite PSNR and create diff)..."
	$(PROG) 1.png 2.png diff-1-2.png

test-unrelated:
	@echo "Testing unrelated images (should report a low PSNR and create diff)..."
	$(PROG) 1.png 3.png diff-1-3.png

clean:
	rm -f diff-*.png
