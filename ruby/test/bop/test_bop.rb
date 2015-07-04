print("In test_bop\n");
PPR.verbose(3)
PPR.set_group_size(4)
def calc(n)
  sum = 0
  for i in 0..n
    sum +=i
  end
  return sum
end


print(PPR.to_s)
for i in 0..3
  print(PPR.new{calc(i*100000)}.call)
end
print("Finished test_bop\n");
