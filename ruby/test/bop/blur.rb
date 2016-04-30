class Pixel
  attr_reader :a, :b, :c
  def initialize(a=0,b=0,c=0)
    @a = a
    @b = b
    @c = c
    @avd = 0
  end
  def add(p)
    @a += p.a
    @b += p.b
    @c += p.c
    @avd +=1
  end

  def average
    @a /= @avd
    @b /= @avd
    @c /= @avd
    return self
  end

  def to_s
    "r=#{@a} b=#{@b} g=#{@c}"
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

def blur(read_index, x, y, range=3)
  sleep(0.0005)
  # return Pixel.new #$images[read_index][x][y]
  max = $size - 1
  avg = Pixel.new
  base_row = [x-range, 0].max
  high_row = [x+range, max].min
  base_col = [y-range, 0].max
  high_col = [y+range, max].min
  (base_row .. high_row).each do |row|
    (base_col..high_col).each do |col|
      avg.add $images[read_index][row][col]
    end
  end
  return avg
end


read_index = 1
write_index = 0
per_task = PPR.ppr_index == -1 ? 1 : [$size / PPR.get_group_size, 1].max
1.times do
  read_index = (read_index+1) % 2
  write_index = (read_index-1) % 2
  PPR.get_group_size.times { |spec_ind|
    PPR {
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
  PPR.over
end

# puts $images[write_index]
