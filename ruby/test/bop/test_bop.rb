print("In test_bop\n");
PPR.verbose(3)
PPR.set_group_size(4)
for i in 0..3
  PPR.new{print("In ppr task\n"); sleep(1)}.call;
end
print("Finished test_bop\n");
