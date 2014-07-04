@Echo off
SET PATH=sys
copy Game\1ST_READ.BIN sys\1ST_READ.BIN
echo Descrambling 1ST_READ.BIN ...
scramble -d Game\1ST_READ.BIN Game\unscrambled.bin
del Game\1ST_READ.BIN
move Game\unscrambled.bin Game\1ST_READ.BIN
echo Creating ISO...
mkisofs -V DC_APP -G sys/homebrew/IP.BIN -joliet -rock -l -o Lights.iso ./Game
del Game\1ST_READ.BIN
move sys\1ST_READ.BIN Game\1ST_READ.BIN
echo Complete!