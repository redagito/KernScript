for DIRECTORY in source include
do
	echo "Formatting code in $DIRECTORY/"
	find "$DIRECTORY" \( -name '*.h' -or -name '*.cpp' \) -print0 | xargs -0 clang-format -i
done
