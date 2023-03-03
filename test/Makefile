QMAKE=qmake
LRELEASE=lrelease

test:
	cd send-msg && $(QMAKE) && make
	#cd send-msg && $(LRELEASE) sendmsg.pro

clean:
	cd send-msg && make clean
	-rm send-msg/sendmsg
	-rm send-msg/Makefile

