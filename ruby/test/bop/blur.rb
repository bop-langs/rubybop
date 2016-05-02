class Pixel
  attr_reader :arr
  def initialize(a=0,b=0,c=0)
    @arr=[a,b,c]
    @avd = 0
  end
  def add(p)
    add(p.arr[0], p.arr[1], p.arr[2], 1)
  end
  def add(a,b,c,count)
    @arr[0] += a
    @arr[1] += a
    @arr[2] += a
    @avd +=count
  end

  def average
    @arr[0] /= @avd
    @arr[1] /= @avd
    @arr[2] /= @avd
    return self
  end

  def to_s
    "r=#{@arr[0]} b=#{@arr[1]} g=#{@arr[2]}"
  end

end

$size = ARGV[1].to_i

image = Array.new($size) { Array.new($size) }
image2 = Array.new($size) { Array.new($size) }
$images = Array.new(2) {Array.new($size) { Array.new($size) } }
File.open(ARGV[0],"r").each_line do |line|
  linetmp = line.split()
  x = linetmp[0].to_i
  y = linetmp[1].to_i
  a = linetmp[2].to_i
  b = linetmp[3].to_i
  c = linetmp[4].to_i
  a2 = linetmp[2].to_i
  b2 = linetmp[3].to_i
  c2 = linetmp[4].to_i
  $images[0][x][y] = Pixel.new( a, b, c)
  $images[1][x][y] = Pixel.new( a2, b2, c2)
end

def max(a, b)
  return a > b ? a : b
end
def min(a, b)
  return a < b ? a : b
end

def blur(read_index, x, y, range=3)
  sleep(0.0005)
  # return Pixel.new #$images[read_index][x][y]
  max = $size - 1
  base_row = max(0, x-range) #[x-range, 0].max
  high_row = min(max, x+range) #[x+range, max].min
  base_col = max(0, y-range) #[y-range, 0].max
  high_col = min(max, y+range) #[y+range, max].min
  a, b, c, count = 0,0,0,0.0
  (base_row .. high_row).each do |row|
    (base_col..high_col).each do |col|
      a += $images[read_index][row][col].arr[0]
      b += $images[read_index][row][col].arr[1]
      c += $images[read_index][row][col].arr[2]
      count+=1
    end
  end
  return Pixel.new(a/count, b/count, c/count)
end

def group_size
  return PPR.get_group_size rescue return 1
end
def safe_spec
  PPR{ yield } rescue yield
end

read_index = 0
write_index = 1
per_task = PPR.ppr_index == -1 ? 1 : [$size / PPR.get_group_size, 1].max rescue $size
10.times do
  read_index = (read_index+1) % 2
  write_index = (read_index-1) % 2
  group_size.times { |spec_ind|
    safe_spec {
      per_task.times { |ppr_loop|
        row = (spec_ind * per_task) + ppr_loop
        if row < $size then
          $size.times { |col|
            $images[write_index][row][col] = blur(read_index, row, col)
          }
        end
      }
    }
  }
  PPR.over rescue -> {}
end

puts $images[write_index]
