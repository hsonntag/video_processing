
for i=1:100
    pause(0.01);
    im = read_camera(1);%uint8(zeros(300, 200, 3));%
    imshow(im);
end
read_camera(-1);
